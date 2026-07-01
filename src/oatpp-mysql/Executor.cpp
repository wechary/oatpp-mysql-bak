#include "Executor.hpp"

#include "ql_template/Parser.hpp"
#include "ql_template/TemplateValueProvider.hpp"

#include "oatpp/core/data/mapping/type/Collection.hpp"
#include "oatpp/core/data/mapping/type/Vector.hpp"

namespace {

oatpp::String buildPreparedTemplate(const oatpp::orm::Executor::StringTemplate& queryTemplate,
                                    const std::unordered_map<oatpp::String, oatpp::Void>& params,
                                    const std::shared_ptr<const oatpp::data::mapping::TypeResolver>& typeResolver) {
  oatpp::data::mapping::TypeResolver::Cache cache;
  auto extra = std::static_pointer_cast<oatpp::mysql::ql_template::Parser::TemplateExtra>(queryTemplate.getExtraData());

  auto result = std::make_shared<std::string>();
  auto currentPos = 0u;
  const auto& variables = queryTemplate.getTemplateVariables();

  for (const auto& var : variables) {
    result->append(extra->templateText->substr(currentPos, var.posStart - currentPos));
    currentPos = var.posStart + var.name->size() + 1;

    auto parser = oatpp::parser::Caret(var.name);
    auto nameLabel = parser.putLabel();
    std::vector<std::string> propertyPath;
    if(parser.findChar('.') && parser.getPosition() < parser.getDataSize() - 1) {
      do {
        parser.inc();
        auto label = parser.putLabel();
        parser.findChar('.');
        propertyPath.push_back(label.std_str());
      } while (parser.getPosition() < parser.getDataSize());
    }

    auto paramName = nameLabel.toString();
    if (!paramName->empty()) {
      auto it = params.find(paramName);
      if (it != params.end()) {
        auto value = typeResolver->resolveObjectPropertyValue(it->second, propertyPath, cache);
        if (value.getValueType()->isCollection) {
          auto dispatcher = static_cast<const oatpp::data::mapping::type::__class::Collection::PolymorphicDispatcher*>(value.getValueType()->polymorphicDispatcher);
          auto size = dispatcher->getCollectionSize(value);
          if (size > 0) {
            for (v_int64 i = 0; i < size; ++i) {
              if (i > 0) {
                result->append(",");
              }
              result->append("?");
            }
          } else {
            result->append("?");
          }
        } else {
          result->append("?");
        }
      } else {
        result->append("?");
      }
    } else {
      result->append("?");
    }
  }

  result->append(extra->templateText->substr(currentPos));
  return result;
}

}

namespace oatpp { namespace mysql {

void Executor::ConnectionInvalidator::invalidate(const std::shared_ptr<orm::Connection>& connection) {
  auto c = std::static_pointer_cast<Connection>(connection);
  auto invalidator = c->getInvalidator();
  if(!invalidator) {
    throw std::runtime_error("[oatpp::mysql::Executor::ConnectionInvalidator::invalidate()]: Error. "
                             "Connection invalidator was NOT set.");
  }
  invalidator->invalidate(c);
}

Executor::Executor(const std::shared_ptr<provider::Provider<Connection>>& connectionProvider)
  : m_connectionProvider(connectionProvider)
  , m_connectionInvalidator(std::make_shared<ConnectionInvalidator>())
  , m_serializer(std::make_shared<mapping::Serializer>())
  , m_resultMapper(std::make_shared<mapping::ResultMapper>())
{

}

std::shared_ptr<data::mapping::TypeResolver> Executor::createTypeResolver() {
  auto resolver = std::make_shared<data::mapping::TypeResolver>();
  return resolver;
}

provider::ResourceHandle<orm::Connection> Executor::getConnection() {
  auto connection = m_connectionProvider->get();
  if (connection) {
    connection.object->setInvalidator(connection.invalidator);
    return provider::ResourceHandle<orm::Connection>(
      connection.object,
      m_connectionInvalidator
    );
  }
  throw std::runtime_error("[oatpp::mysql::Executor::getConnection()]: Error. Can't connect.");
}

data::share::StringTemplate Executor::parseQueryTemplate(const oatpp::String& name,
                                                         const oatpp::String& text,
                                                         const ParamsTypeMap& paramsTypeMap,
                                                         bool prepare) {
  (void) paramsTypeMap;

  auto&& t = ql_template::Parser::parseTemplate(text);

  auto extra = std::make_shared<ql_template::Parser::TemplateExtra>();
  t.setExtraData(extra);

  extra->prepare = prepare;
  extra->templateName = name;
  extra->templateText = text;

  ql_template::TemplateValueProvider valueProvider;
  extra->preparedTemplate = t.format(&valueProvider);
  extra->placeholderCount = valueProvider.getPlaceholderCount();

  return t;
}

// e.g. "user.name.first" -> QueryParameter{name="user", propertyPath={"name", "first"}}
Executor::QueryParameter Executor::parseQueryParameter(const oatpp::String& paramName) {

  parser::Caret caret(paramName);
  auto nameLabel = caret.putLabel();
  if(caret.findChar('.') && caret.getPosition() < caret.getDataSize() - 1) {

    QueryParameter result;
    result.name = nameLabel.toString();

    do {

      caret.inc();
      auto label = caret.putLabel();
      caret.findChar('.');
      result.propertyPath.push_back(label.std_str());

    } while (caret.getPosition() < caret.getDataSize());

    return result;

  }

  return {nameLabel.toString(), {}};

}

// mysql bind params
void Executor::bindParams(MYSQL_STMT* stmt,
                          const StringTemplate& queryTemplate,
                          const std::unordered_map<oatpp::String, oatpp::Void>& params, 
                          const std::shared_ptr<const data::mapping::TypeResolver>& typeResolver) {
  data::mapping::TypeResolver::Cache cache;

  auto extra = std::static_pointer_cast<ql_template::Parser::TemplateExtra>(queryTemplate.getExtraData());

  m_serializer->clearBindParams();

  size_t count = queryTemplate.getTemplateVariables().size();
  size_t bindIndex = 0;
  for (size_t i = 0; i < count; ++i) {
    auto& var = queryTemplate.getTemplateVariables()[i];
    
    auto queryParam = parseQueryParameter(var.name);  // e.g. "user.name.first" -> QueryParameter{name="user", propertyPath={"name", "first"}}

    if (queryParam.name->empty()) {
      throw std::runtime_error("[oatpp::mysql::Executor::bindParams()]: Error. "
        "Can't parse query parameter name. Parameter name: " + var.name);
    }

    // resolve parameter type
    auto it = params.find(queryParam.name);
    if (it != params.end()) {
      auto value = typeResolver->resolveObjectPropertyValue(it->second, queryParam.propertyPath, cache);
      if (value.getValueType()->classId.id == oatpp::Void::Class::CLASS_ID.id) {
        throw std::runtime_error("[oatpp::mysql::Executor::bindParams()]: Error. "
          "Can't resolve parameter type because property dose not found or its type is unknown." 
          " Parameter name: " + queryParam.name + ", var.name: " + var.name);
      }

      if (value.getValueType()->isCollection) {
        auto dispatcher = static_cast<const data::mapping::type::__class::Collection::PolymorphicDispatcher*>(value.getValueType()->polymorphicDispatcher);
        auto size = dispatcher->getCollectionSize(value);
        if (size > 0) {
          auto iterator = dispatcher->beginIteration(value);
          while(!iterator->finished()) {
            auto item = iterator->get();
            m_serializer->serialize(stmt, bindIndex++, item);
            iterator->next();
          }
        } else {
          m_serializer->serialize(stmt, bindIndex++, oatpp::Void());
        }
      } else {
        m_serializer->serialize(stmt, bindIndex++, value);
      }
    }
  }

  auto& bindParams = m_serializer->getBindParams();
  // OATPP_LOGD("EXECUTOR", "MYSQL_BIND count: %zu", bindParams.size());
  // for (size_t i = 0; i < bindParams.size(); i++) {
  //     OATPP_LOGD("EXECUTOR", "  Bind[%zu]: type=%d, buffer=%p, is_null=%d", 
  //               i, bindParams[i].buffer_type, 
  //               bindParams[i].buffer,
  //               bindParams[i].is_null ? *bindParams[i].is_null : -1);
  // }
  int rc = mysql_stmt_bind_param(stmt, bindParams.data());
  // OATPP_LOGD("EXECUTOR", "mysql_stmt_bind_param result: %d", rc);
  if (rc) {
    throw std::runtime_error("[oatpp::mysql::Executor::bindParams()]: Error. "
      "Can't bind parameters. Error: " + std::string(mysql_stmt_error(stmt)));
  }
}

std::shared_ptr<orm::QueryResult> Executor::execute(const StringTemplate& queryTemplate,
                                                    const std::unordered_map<oatpp::String, oatpp::Void>& params,
                                                    const std::shared_ptr<const data::mapping::TypeResolver>& typeResolver,
                                                    const provider::ResourceHandle<orm::Connection>& connection)
{
  auto connectionHandle = connection;
  if (!connectionHandle) {
    connectionHandle = getConnection();
  }

  std::shared_ptr<const data::mapping::TypeResolver> tr = typeResolver;
  if(!tr) {
    tr = m_defaultTypeResolver;
  }

  auto mysqlConnection = std::static_pointer_cast<mysql::Connection>(connectionHandle.object);

  MYSQL_STMT* stmt = mysql_stmt_init(mysqlConnection->getHandle());
  if (!stmt) {
    throw std::runtime_error("[oatpp::mysql::Executor::execute()]: "
      "ErrorError. Can't create MYSQL_STMT. Error: " + std::string(mysql_error(mysqlConnection->getHandle())));
  }

  auto extra = std::static_pointer_cast<ql_template::Parser::TemplateExtra>(queryTemplate.getExtraData());
  auto preparedTemplate = buildPreparedTemplate(queryTemplate, params, tr);
  extra->preparedTemplate = preparedTemplate;

  if (mysql_stmt_prepare(stmt, extra->preparedTemplate->c_str(), extra->preparedTemplate->size())) {
    throw std::runtime_error("[oatpp::mysql::Executor::execute()]: "
      "Error. Can't prepare MYSQL_STMT. preparedTemplate: " + extra->preparedTemplate +
      " Error: " + std::string(mysql_stmt_error(stmt)));
  }

  bindParams(stmt, queryTemplate, params, tr);

  return std::make_shared<mysql::QueryResult>(stmt, connectionHandle, m_resultMapper, tr);
}

std::shared_ptr<orm::QueryResult> Executor::begin(const provider::ResourceHandle<orm::Connection>& connection) {
  throw std::runtime_error("[oatpp::mysql::Executor::begin()]: "
                           "Error. Not implemented.");
}

std::shared_ptr<orm::QueryResult> Executor::commit(const provider::ResourceHandle<orm::Connection>& connection) {
  throw std::runtime_error("[oatpp::mysql::Executor::commit()]: "
                           "Error. Not implemented.");
}

std::shared_ptr<orm::QueryResult> Executor::rollback(const provider::ResourceHandle<orm::Connection>& connection) {
  throw std::runtime_error("[oatpp::mysql::Executor::rollback()]: "
                           "Error. Not implemented.");
}

v_int64 Executor::getSchemaVersion(const oatpp::String& suffix,
                                   const provider::ResourceHandle<orm::Connection>& connection)
{
  throw std::runtime_error("[oatpp::mysql::Executor::getSchemaVersion()]: "
                           "Error. Not implemented.");
}

void Executor::migrateSchema(const oatpp::String& script,
                             v_int64 newVersion,
                             const oatpp::String& suffix,
                             const provider::ResourceHandle<orm::Connection>& connection)
{
  throw std::runtime_error("[oatpp::mysql::Executor::migrateSchema()]: "
                           "Error. Not implemented.");
}

}}