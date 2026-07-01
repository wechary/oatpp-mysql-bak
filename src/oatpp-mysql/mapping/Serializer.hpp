#ifndef oatpp_mysql_mapping_Serializer_hpp
#define oatpp_mysql_mapping_Serializer_hpp

#include "oatpp/core/Types.hpp"
#include <mysql/mysql.h>

namespace oatpp { namespace mysql { namespace mapping {

/**
 * Mapper of oatpp values to mysql values.
 */
class Serializer {
public:
  typedef void (*SerializerMethod)(const Serializer*, MYSQL_STMT*, v_uint32, const oatpp::Void&);
private:
  std::vector<SerializerMethod> m_methods;
  mutable std::vector<MYSQL_BIND> m_bindParams;
public:

  Serializer();

  ~Serializer();

  void setSerializerMethod(const data::mapping::type::ClassId& classId, SerializerMethod method);

  void serialize(MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph) const;

  std::vector<MYSQL_BIND>& getBindParams() const;

  void clearBindParams() const;

private:

  void setBindParam(MYSQL_BIND& bind, v_uint32 paramIndex) const;

private:

  static void serializeString(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeNull(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeInt8(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeUInt8(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeInt16(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeUInt16(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeInt32(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeUInt32(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeInt64(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeUInt64(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeFloat32(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeFloat64(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

  static void serializeEnum(const Serializer* _this, MYSQL_STMT* stmt, v_uint32 paramIndex, const oatpp::Void& polymorph);

};

}}}

#endif // oatpp_mysql_mapping_Serializer_hpp
