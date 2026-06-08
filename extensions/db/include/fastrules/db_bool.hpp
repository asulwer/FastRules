// ============================================================================
// db_bool.hpp
// SOCI bool abstraction for fastrules-db
//
// SOCI does not natively support bool in its exchange_type enum. This module
// provides a DbBool wrapper with a custom type_conversion so fastrules users
// can bind bool values while SOCI stores them as int(1) in the database.
//
// Usage:
//   DbBool active = true;
//   *session << "INSERT INTO rules (is_active) VALUES (:val)", soci::use(active);
//   *session << "SELECT is_active FROM rules WHERE id = :id", soci::into(active);
// ============================================================================

#pragma once

#include <soci/soci.h>
#include <soci/type-conversion-traits.h>

namespace fastrules {
namespace ext {

struct DbBool {
    bool value;
    DbBool() : value(false) {}
    DbBool(bool v) : value(v) {}
    operator bool() const { return value; }
    DbBool& operator=(bool v) { value = v; return *this; }
};

} // namespace ext
} // namespace fastrules

namespace soci {

template<>
struct type_conversion<fastrules::ext::DbBool> {
    typedef int base_type;
    static void from_base(const int& v, indicator ind, fastrules::ext::DbBool& result) {
        if (ind == i_null) {
            result.value = false;
        } else {
            result.value = (v != 0);
        }
    }
    static void to_base(const fastrules::ext::DbBool& v, int& result, indicator& ind) {
        result = v.value ? 1 : 0;
        ind = i_ok;
    }
};

} // namespace soci