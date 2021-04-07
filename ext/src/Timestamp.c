/**
 * Copyright 2015-2017 DataStax, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "php_driver.h"
#include "php_driver_types.h"
#include "util/hash.h"
#include "util/types.h"
#include <ext/date/php_date.h>

zend_class_entry *php_driver_timestamp_ce = NULL;

void
php_driver_timestamp_init(INTERNAL_FUNCTION_PARAMETERS)
{
  cass_int64_t seconds = 0;
  cass_int64_t microseconds = 0;
  php_driver_timestamp *self;
  cass_int64_t value = 0;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|ll", &seconds, &microseconds) == FAILURE) {
    return;
  }

  if (ZEND_NUM_ARGS() == 0) {
#ifdef WIN32
    seconds = (cass_int64_t) time(0);
#else
    struct timeval time;

    gettimeofday(&time, NULL);
    seconds = time.tv_sec;
    microseconds = (time.tv_usec / 1000) * 1000;
#endif
  }

  value += microseconds / 1000;
  value += (seconds * 1000);

  if (getThis() && instanceof_function(Z_OBJCE_P(getThis()), php_driver_timestamp_ce)) {
    self = PHP_DRIVER_GET_TIMESTAMP(getThis());
  } else {
    object_init_ex(return_value, php_driver_timestamp_ce);
    self = PHP_DRIVER_GET_TIMESTAMP(return_value);
  }

  self->timestamp = value;
}

/* {{{ Timestamp::__construct(string) */
PHP_METHOD(Timestamp, __construct)
{
  php_driver_timestamp_init(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Timestamp::type() */
PHP_METHOD(Timestamp, type)
{
  zval type = php_driver_type_scalar(CASS_VALUE_TYPE_TIMESTAMP);
  RETURN_ZVAL(&(type), 1, 1);
}
/* }}} */

/* {{{ Timestamp::time */
PHP_METHOD(Timestamp, time)
{
  php_driver_timestamp *self = PHP_DRIVER_GET_TIMESTAMP(getThis());

  RETURN_LONG(self->timestamp / 1000);
}
/* }}} */

/* {{{ Timestamp::microtime(bool) */
PHP_METHOD(Timestamp, microtime)
{
  zend_bool get_as_float = 0;
  php_driver_timestamp *self;
  char *ret = NULL;
  long sec = -1;
  double usec = 0.0f;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &get_as_float) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TIMESTAMP(getThis());

  if (get_as_float) {
    RETURN_DOUBLE((double) self->timestamp / 1000.00);
  }

  sec    = (long) (self->timestamp / 1000);
  usec   = (double) ((self->timestamp - (sec * 1000)) / 1000.00);
  spprintf(&ret, 0, "%.8F %ld", usec, sec);
  RETVAL_STRING(ret);
  efree(ret);
}
/* }}} */

/* {{{ Timestamp::toDateTime() */
PHP_METHOD(Timestamp, toDateTime)
{
  php_driver_timestamp *self;
  zval datetime_object;
  zval *datetime = &datetime_object;
  php_date_obj *datetime_obj = NULL;
  char *str;
  int str_len;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TIMESTAMP(getThis());


  php_date_instantiate(php_date_get_date_ce(), datetime);

  datetime_obj = php_date_obj_from_obj(Z_OBJ_P(datetime));

  str_len      = spprintf(&str, 0, "@%ld", (long) (self->timestamp / 1000));
  php_date_initialize(datetime_obj, str, str_len, NULL, NULL, 0);
  efree(str);

  RETVAL_ZVAL(datetime, 0, 1);
}
/* }}} */

/* {{{ Timestamp::__toString() */
PHP_METHOD(Timestamp, __toString)
{
  php_driver_timestamp *self;
  char *ret = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TIMESTAMP(getThis());

  spprintf(&ret, 0, "%" PRId64, self->timestamp);
  RETVAL_STRING(ret);
  efree(ret);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, seconds)
  ZEND_ARG_INFO(0, microseconds)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_microtime, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, get_as_float)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_timestamp_methods[] = {
  PHP_ME(Timestamp, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Timestamp, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Timestamp, time, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Timestamp, microtime, arginfo_microtime, ZEND_ACC_PUBLIC)
  PHP_ME(Timestamp, toDateTime, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Timestamp, __toString, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_timestamp_handlers;

static HashTable *
php_driver_timestamp_gc(zval *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_timestamp_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval type;
  zval seconds;
  zval microseconds;

  php_driver_timestamp *self = CASS_COMPAT_GET_TIMESTAMP(object);
  HashTable           *props = zend_std_get_properties(object);

  long sec  = (long) (self->timestamp / 1000);
  long usec = (long) ((self->timestamp - (sec * 1000)) * 1000);

  type = php_driver_type_scalar(CASS_VALUE_TYPE_TIMESTAMP);
  zend_hash_str_update(props, "type", strlen("type"), &(type));


  ZVAL_LONG(&(seconds), sec);
  zend_hash_str_update(props, "seconds", strlen("seconds"), &(seconds));


  ZVAL_LONG(&(microseconds), usec);
  zend_hash_str_update(props, "microseconds", strlen("microseconds"), &(microseconds));

  return props;
}

static int
php_driver_timestamp_compare(zval *obj1, zval *obj2)
{
  php_driver_timestamp *timestamp1 = NULL;
  php_driver_timestamp *timestamp2 = NULL;
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  timestamp1 = PHP_DRIVER_GET_TIMESTAMP(obj1);
  timestamp2 = PHP_DRIVER_GET_TIMESTAMP(obj2);

  return PHP_DRIVER_COMPARE(timestamp1->timestamp, timestamp2->timestamp);
}

static unsigned
php_driver_timestamp_hash_value(zval *obj)
{
  php_driver_timestamp *self = PHP_DRIVER_GET_TIMESTAMP(obj);
  return php_driver_bigint_hash(self->timestamp);
}

static void
php_driver_timestamp_free(zend_object *object)
{
  php_driver_timestamp *self = php_driver_timestamp_object_fetch(object);;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_timestamp_new(zend_class_entry *ce)
{
  php_driver_timestamp *self =
      CASS_ZEND_OBJECT_ECALLOC(timestamp, ce);

  CASS_ZEND_OBJECT_INIT(timestamp, self, ce);
}

void php_driver_define_Timestamp()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Timestamp", php_driver_timestamp_methods);
  php_driver_timestamp_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_timestamp_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_timestamp_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_timestamp_handlers.std.get_properties  = php_driver_timestamp_properties;
  php_driver_timestamp_handlers.std.get_gc          = php_driver_timestamp_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_timestamp_handlers.std, php_driver_timestamp_compare);
  php_driver_timestamp_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_timestamp_ce->create_object = php_driver_timestamp_new;

  php_driver_timestamp_handlers.hash_value = php_driver_timestamp_hash_value;
  php_driver_timestamp_handlers.std.clone_obj = NULL;
}
