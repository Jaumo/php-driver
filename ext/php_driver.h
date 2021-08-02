#ifndef PHP_DRIVER_H
#define PHP_DRIVER_H

#ifdef HAVE_CONFIG_H

#  include "config.h"

#endif

#include <gmp.h>
#include <cassandra.h>

/* Ensure Visual Studio 2010 does not load MSVC++ stdint definitions */
#ifdef _WIN32
#  ifdef DISABLE_MSVC_STDINT
#    pragma once
#    ifndef _STDINT
#      define _STDINT
#    endif
#  endif
#endif

#include <php.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_interfaces.h>
#include <Zend/zend_types.h>

#ifdef HAVE_SYS_TYPES_H

#include <sys/types.h>

#endif

#ifdef HAVE_UNISTD_H

#include <unistd.h>

#endif

#ifdef PHP_WIN32
typedef int pid_t;
#include <process.h>
#endif

#ifdef WIN32
#  define LL_FORMAT "%I64d"
#else
#  define LL_FORMAT "%" PRId64 ""
#endif

#if PHP_VERSION_ID < 50600
#  error PHP 5.6.0 or later is required in order to build the driver
#endif

#include <ext/spl/spl_iterators.h>
#include <ext/spl/spl_exceptions.h>

#include "version.h"

#ifdef PHP_WIN32
#  define PHP_DRIVER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define PHP_DRIVER_API __attribute__ ((visibility("default")))
#else
#  define PHP_DRIVER_API
#endif

#define PHP_DRIVER_NAMESPACE "Cassandra"

#define PHP_DRIVER_NAMESPACE_ZEND_ARG_OBJ_INFO(pass_by_ref, name, classname, allow_null) \
  ZEND_ARG_OBJ_INFO(pass_by_ref, name, Cassandra\\classname, allow_null)

#define PHP_DRIVER_CORE_METHOD(name) \
  PHP_METHOD(Cassandra, name)

#define PHP_DRIVER_CORE_ME(name, arg_info, flags) \
  PHP_ME(Cassandra, name, arg_info, flags)

#define SAFE_STR(a) ((a)?a:"")

#define PHP_DRIVER_G(v) (php_driver_globals.v)

#define CPP_DRIVER_VERSION(major, minor, patch) \
  (((major) << 16) + ((minor) << 8) + (patch))

#define CURRENT_CPP_DRIVER_VERSION \
  CPP_DRIVER_VERSION(CASS_VERSION_MAJOR, CASS_VERSION_MINOR, CASS_VERSION_PATCH)

static inline int
cass_string_compare(zend_string *s1, zend_string *s2)
{
  if (s1->len != s2->len) {
    return s1->len < s2->len ? -1 : 1;
  }
  return memcmp(s1->val, s2->val, s1->len);
}

#define CASS_SMART_STR_VAL(ss) ((ss).s ? (ss).s->val : NULL)
#define CASS_SMART_STR_LEN(ss) ((ss).s ? (ss).s->len : 0)

#define CASS_ZEND_OBJECT_ECALLOC(type_name, ce) \
  (php_driver_##type_name *) ecalloc(1, sizeof(php_driver_##type_name) + zend_object_properties_size(ce))

#define CASS_ZEND_OBJECT_INIT(type_name, self, ce) \
  CASS_ZEND_OBJECT_INIT_EX(type_name, type_name, self, ce)

#define CASS_ZEND_OBJECT_INIT_EX(type_name, name, self, ce) do {             \
  zend_object_std_init(&self->zval, ce);                              \
  ((zend_object_handlers *) &php_driver_##name##_handlers)->offset =             \
        XtOffsetOf(php_driver_##type_name, zval);                                \
  ((zend_object_handlers *) &php_driver_##name##_handlers)->free_obj =           \
        php_driver_##name##_free;                                            \
  self->zval.handlers = (zend_object_handlers *) &php_driver_##name##_handlers;  \
  return &self->zval;                                                           \
} while(0)

#define CASS_ZEND_HASH_FOREACH_STR_KEY_VAL(ht, _key, _val) \
  ZEND_HASH_FOREACH(ht, 0);                                   \
  if (_p->key) {                                              \
    (_key) = _p->key->val;                                    \
  }  else {                                                   \
    (_key) = NULL;                                            \
  }                                                           \
  _val = _z;

#define CASS_ZEND_HASH_GET_CURRENT_DATA(ht, res) \
  ((res = zend_hash_get_current_data((ht))) != NULL)

#define CASS_ZEND_HASH_GET_CURRENT_DATA_EX(ht, res, pos) \
  ((res = zend_hash_get_current_data_ex((ht), (pos))) != NULL)

#define CASS_ZEND_HASH_FIND(ht, key, len, res) \
  ((res = zend_hash_str_find((ht), (key), (size_t)(len - 1))) != NULL)

#define CASS_ZEND_HASH_INDEX_FIND(ht, index, res) \
  ((res = zend_hash_index_find((ht), (zend_ulong) (index))) != NULL)

#define CASS_ZEND_HASH_ZVAL_COPY(dst, src) \
  zend_hash_copy(dst, src, (copy_ctor_func_t) zval_add_ref);

#define CASS_ZVAL_IS_BOOL_P(zv) \
  (Z_TYPE_P(zv) == IS_TRUE  || Z_TYPE_P(zv) == IS_FALSE)

#define CASS_ZVAL_MAYBE_DESTROY(zv) do { \
  if (!Z_ISUNDEF(zv)) {                     \
    zval_ptr_dtor(&(zv));                   \
    ZVAL_UNDEF(&(zv));                      \
  }                                         \
} while(0)


#if PHP_VERSION_ID >= 80100
#define CASS_COMPAT_STREAM_STR_FROM_CHAR(c) zend_string_init(c, strlen(c), 0)

#else
#define CASS_COMPAT_STREAM_STR_FROM_CHAR(c) c
#endif


#if PHP_VERSION_ID >= 80000
#define CASS_COMPAT_GET_NUMERIC(obj) php_driver_numeric_object_fetch(obj)
#define CASS_COMPAT_GET_TYPE(obj) php_driver_type_object_fetch(obj)
#define CASS_COMPAT_GET_BLOB(obj) php_driver_blob_object_fetch(obj)
#define CASS_COMPAT_GET_COLLECTION(obj) php_driver_collection_object_fetch(obj)
#define CASS_COMPAT_GET_DATE(obj) php_driver_date_object_fetch(obj)
#define CASS_COMPAT_GET_DURATION(obj) php_driver_duration_object_fetch(obj)
#define CASS_COMPAT_GET_INET(obj) php_driver_inet_object_fetch(obj)
#define CASS_COMPAT_GET_MAP(obj) php_driver_map_object_fetch(obj)
#define CASS_COMPAT_GET_SET(obj) php_driver_set_object_fetch(obj)
#define CASS_COMPAT_GET_TIME(obj) php_driver_time_object_fetch(obj)
#define CASS_COMPAT_GET_TIMESTAMP(obj) php_driver_timestamp_object_fetch(obj)
#define CASS_COMPAT_GET_UUID(obj) php_driver_uuid_object_fetch(obj)
#define CASS_COMPAT_GET_TUPLE(obj) php_driver_tuple_object_fetch(obj)
#define CASS_COMPAT_GET_USER_TYPE_VALUE(obj) php_driver_user_type_value_object_fetch(obj)
#define CASS_COMPAT_GET_CLUSTER_BUILDER(obj) php_driver_cluster_builder_object_fetch(obj)

#define CASS_COMPAT_OBJECT_HANDLER_TYPE zend_object
#define CASS_COMPAT_SET_COMPARE_HANDLER(ref, handler) ref.compare = handler
#define CASS_COMPAT_zend_call_method_with_0_params(obj, obj_ce, fn_proxy, function_name, retval) \
  zend_call_method_with_0_params(Z_OBJ_P(obj), obj_ce, fn_proxy, function_name, retval)
#define CASS_COMPAT_HASH_SORT_TYPE Bucket*
#else
#define CASS_COMPAT_GET_NUMERIC(obj) php_driver_numeric_object_fetch(Z_OBJ_P(obj))
#define CASS_COMPAT_GET_TYPE(obj) PHP_DRIVER_GET_TYPE(obj)
#define CASS_COMPAT_GET_BLOB(obj) PHP_DRIVER_GET_BLOB(obj)
#define CASS_COMPAT_GET_COLLECTION(obj) PHP_DRIVER_GET_COLLECTION(obj)
#define CASS_COMPAT_GET_DATE(obj) PHP_DRIVER_GET_DATE(obj)
#define CASS_COMPAT_GET_DURATION(obj) PHP_DRIVER_GET_DURATION(obj)
#define CASS_COMPAT_GET_INET(obj) PHP_DRIVER_GET_INET(obj)
#define CASS_COMPAT_GET_MAP(obj) PHP_DRIVER_GET_MAP(obj)
#define CASS_COMPAT_GET_SET(obj) PHP_DRIVER_GET_SET(obj)
#define CASS_COMPAT_GET_TIME(obj) PHP_DRIVER_GET_TIME(obj)
#define CASS_COMPAT_GET_TIMESTAMP(obj) PHP_DRIVER_GET_TIMESTAMP(obj)
#define CASS_COMPAT_GET_UUID(obj) PHP_DRIVER_GET_UUID(obj)
#define CASS_COMPAT_GET_TUPLE(obj) PHP_DRIVER_GET_TUPLE(obj)
#define CASS_COMPAT_GET_USER_TYPE_VALUE(obj) PHP_DRIVER_GET_USER_TYPE_VALUE(obj)
#define CASS_COMPAT_GET_CLUSTER_BUILDER(obj) PHP_DRIVER_GET_CLUSTER_BUILDER(obj)

#define CASS_COMPAT_OBJECT_HANDLER_TYPE zval
#define CASS_COMPAT_SET_COMPARE_HANDLER(ref, handler) ref.compare_objects = handler
#define ZEND_COMPARE_OBJECTS_FALLBACK(op1, op2)         \
  if (Z_OBJCE_P(obj1) != Z_OBJCE_P(obj2))               \
    return 1; /* different classes */
#define CASS_COMPAT_zend_call_method_with_0_params(obj, obj_ce, fn_proxy, function_name, retval) \
  zend_call_method_with_0_params(obj, obj_ce, fn_proxy, function_name, retval)
#define CASS_COMPAT_HASH_SORT_TYPE const void*
#endif




extern zend_module_entry php_driver_module_entry;

PHP_MINIT_FUNCTION (php_driver);
PHP_MSHUTDOWN_FUNCTION (php_driver);
PHP_RINIT_FUNCTION (php_driver);
PHP_RSHUTDOWN_FUNCTION (php_driver);
PHP_MINFO_FUNCTION (php_driver);

zend_class_entry *exception_class(CassError rc);

void throw_invalid_argument(zval *object,
                            const char *object_name,
                            const char *expected_type);

#define INVALID_ARGUMENT(object, expected) \
{ \
  throw_invalid_argument(object, #object, expected); \
  return; \
}

#define INVALID_ARGUMENT_VALUE(object, expected, failed_value) \
{ \
  throw_invalid_argument(object, #object, expected); \
  return failed_value; \
}

#define ASSERT_SUCCESS_BLOCK(rc, block) \
{ \
  if (rc != CASS_OK) { \
    zend_throw_exception_ex(exception_class(rc), rc, \
                            "%s", cass_error_desc(rc)); \
    block \
  } \
}

#define ASSERT_SUCCESS(rc) ASSERT_SUCCESS_BLOCK(rc, return;)

#define ASSERT_SUCCESS_VALUE(rc, value) ASSERT_SUCCESS_BLOCK(rc, return value;)

#define PHP_DRIVER_DEFAULT_CONSISTENCY CASS_CONSISTENCY_LOCAL_ONE

#define PHP_DRIVER_DEFAULT_LOG       PHP_DRIVER_NAME ".log"
#define PHP_DRIVER_DEFAULT_LOG_LEVEL "ERROR"

#define PHP_DRIVER_INI_ENTRY_LOG \
  PHP_INI_ENTRY(PHP_DRIVER_NAME ".log", PHP_DRIVER_DEFAULT_LOG, PHP_INI_ALL, OnUpdateLog)

#define PHP_DRIVER_INI_ENTRY_LOG_LEVEL \
  PHP_INI_ENTRY(PHP_DRIVER_NAME ".log_level", PHP_DRIVER_DEFAULT_LOG_LEVEL, PHP_INI_ALL, OnUpdateLogLevel)

PHP_INI_MH (OnUpdateLogLevel);

PHP_INI_MH (OnUpdateLog);

#endif /* PHP_DRIVER_H */
