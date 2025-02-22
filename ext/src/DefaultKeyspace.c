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
#include "util/result.h"
#include "util/ref.h"
#include "util/types.h"

#include "DefaultFunction.h"
#include "DefaultMaterializedView.h"
#include "DefaultTable.h"


#include <zend_smart_str.h>

zend_class_entry *php_driver_default_keyspace_ce = NULL;

PHP_METHOD(DefaultKeyspace, name)
{
  php_driver_keyspace *self;
  zval value;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  php_driver_get_keyspace_field(self->meta, "keyspace_name", &value);
  RETURN_ZVAL(&(value), 0, 1);
}

PHP_METHOD(DefaultKeyspace, replicationClassName)
{
  php_driver_keyspace *self;
  zval value;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  php_driver_get_keyspace_field(self->meta, "strategy_class", &value);
  RETURN_ZVAL(&(value), 0, 1);
}

PHP_METHOD(DefaultKeyspace, replicationOptions)
{
  php_driver_keyspace *self;
  zval value;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  php_driver_get_keyspace_field(self->meta, "strategy_options", &value);
  RETURN_ZVAL(&(value), 0, 1);
}

PHP_METHOD(DefaultKeyspace, hasDurableWrites)
{
  php_driver_keyspace *self;
  zval value;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  php_driver_get_keyspace_field(self->meta, "durable_writes", &value);
  RETURN_ZVAL(&(value), 0, 1);
}

PHP_METHOD(DefaultKeyspace, table)
{
  char *name;
  size_t name_len;
  php_driver_keyspace *self;
  zval ztable;
  const CassTableMeta *meta;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_KEYSPACE(getThis());
  meta = cass_keyspace_meta_table_by_name_n(self->meta,
                                            name, name_len);
  if (meta == NULL) {
    RETURN_FALSE;
  }

  ztable = php_driver_create_table(self->schema, meta);
  if (Z_ISUNDEF(ztable)) {
    return;
  }

  RETURN_ZVAL(&(ztable), 0, 1);
}

PHP_METHOD(DefaultKeyspace, tables)
{
  php_driver_keyspace *self;
  CassIterator *iterator;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self     = PHP_DRIVER_GET_KEYSPACE(getThis());
  iterator = cass_iterator_tables_from_keyspace_meta(self->meta);

  array_init(return_value);
  while (cass_iterator_next(iterator)) {
    const CassTableMeta *meta;
    zval ztable;
    php_driver_table *table;

    meta  = cass_iterator_get_table_meta(iterator);
    ztable = php_driver_create_table(self->schema, meta);

    if (Z_ISUNDEF(ztable)) {
      zval_ptr_dtor(return_value);
      cass_iterator_free(iterator);
      return;
    } else {
      table = PHP_DRIVER_GET_TABLE(&(ztable));

      if (Z_TYPE(table->name) == IS_STRING) {
        add_assoc_zval_ex(return_value, Z_STRVAL(table->name), Z_STRLEN(table->name), &(ztable));
      } else {
        add_next_index_zval(return_value, &(ztable));
      }
    }
  }

  cass_iterator_free(iterator);
}

PHP_METHOD(DefaultKeyspace, userType)
{
  char *name;
  size_t name_len;
  php_driver_keyspace *self;
  zval ztype;
  const CassDataType *user_type;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_KEYSPACE(getThis());
  user_type = cass_keyspace_meta_user_type_by_name_n(self->meta, name, name_len);

  if (user_type == NULL) {
    return;
  }

  ztype = php_driver_type_from_data_type(user_type);
  RETURN_ZVAL(&(ztype), 0, 1);
}

PHP_METHOD(DefaultKeyspace, userTypes)
{
  php_driver_keyspace *self;
  CassIterator       *iterator;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self     = PHP_DRIVER_GET_KEYSPACE(getThis());
  iterator = cass_iterator_user_types_from_keyspace_meta(self->meta);

  array_init(return_value);
  while (cass_iterator_next(iterator)) {
    const CassDataType *user_type;
    zval ztype;
    const char *type_name;
    size_t type_name_len;

    user_type = cass_iterator_get_user_type(iterator);
    ztype = php_driver_type_from_data_type(user_type);

    cass_data_type_type_name(user_type, &type_name, &type_name_len);
    add_assoc_zval_ex(return_value, type_name, type_name_len, &(ztype));
  }

  cass_iterator_free(iterator);
}

PHP_METHOD(DefaultKeyspace, materializedView)
{
  php_driver_keyspace *self;
  char *name;
  size_t name_len;
  zval zview;
  const CassMaterializedViewMeta *meta;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_KEYSPACE(getThis());
  meta = cass_keyspace_meta_materialized_view_by_name_n(self->meta,
                                                        name, name_len);
  if (meta == NULL) {
    RETURN_FALSE;
  }

  zview = php_driver_create_materialized_view(self->schema, meta);
  if (Z_ISUNDEF(zview)) {
    return;
  }

  RETURN_ZVAL(&(zview), 0, 1);
}

PHP_METHOD(DefaultKeyspace, materializedViews)
{
  php_driver_keyspace *self;
  CassIterator *iterator;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self     = PHP_DRIVER_GET_KEYSPACE(getThis());
  iterator = cass_iterator_materialized_views_from_keyspace_meta(self->meta);

  array_init(return_value);
  while (cass_iterator_next(iterator)) {
    const CassMaterializedViewMeta *meta;
    zval zview;
    php_driver_materialized_view *view;

    meta  = cass_iterator_get_materialized_view_meta(iterator);
    zview = php_driver_create_materialized_view(self->schema, meta);

    if (Z_ISUNDEF(zview)) {
      zval_ptr_dtor(return_value);
      cass_iterator_free(iterator);
      return;
    } else {
      view = PHP_DRIVER_GET_MATERIALIZED_VIEW(&(zview));

      if (Z_TYPE(view->name) == IS_STRING) {
        add_assoc_zval_ex(return_value, Z_STRVAL(view->name), Z_STRLEN(view->name), &(zview));
      } else {
        add_next_index_zval(return_value, &(zview));
      }
    }
  }

  cass_iterator_free(iterator);
}

int php_driver_arguments_string(zval *args,
                                int argc,
                                smart_str *arguments) {
  int i;

  for (i = 0; i < argc; ++i) {
    zval *argument_type = &(args[i]);

    if (i > 0) {
      smart_str_appendc_ex(arguments, ',', 0);
    }

    if (Z_TYPE_P(argument_type) == IS_STRING) {
      smart_str_appendl_ex(arguments,
                           Z_STRVAL_P(argument_type), Z_STRLEN_P(argument_type),
                           0);
    } else if (Z_TYPE_P(argument_type) == IS_OBJECT &&
               instanceof_function(Z_OBJCE_P(argument_type), php_driver_type_ce)) {
      php_driver_type *type = PHP_DRIVER_GET_TYPE(argument_type);
      php_driver_type_string(type, arguments);
    } else {
      zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                              "Argument types must be either a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
      return FAILURE;
    }
  }

  smart_str_0(arguments);

  return SUCCESS;
}

PHP_METHOD(DefaultKeyspace, function)
{
  php_driver_keyspace *self;
  char *name;
  size_t name_len;
  zval *args = NULL;
  smart_str arguments = {0};
  int argc = 0;
  const CassFunctionMeta *meta = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|*",
                            &name, &name_len,
                            &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  if (argc > 0) {
    if (php_driver_arguments_string(args, argc, &arguments) == FAILURE) {

      return;
    }
  }

  meta =
      cass_keyspace_meta_function_by_name_n(self->meta,
                                            name, name_len,
                                            CASS_SMART_STR_VAL(arguments),
                                            CASS_SMART_STR_LEN(arguments));
  if (meta) {
    zval zfunction = php_driver_create_function(self->schema, meta);
    RETVAL_ZVAL(&(zfunction), 1, 1);
  } else {
    RETVAL_FALSE;
  }

  smart_str_free(&arguments);

}

PHP_METHOD(DefaultKeyspace, functions)
{
  php_driver_keyspace *self;
  CassIterator *iterator;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self     = PHP_DRIVER_GET_KEYSPACE(getThis());
  iterator = cass_iterator_functions_from_keyspace_meta(self->meta);

  array_init(return_value);
  while (cass_iterator_next(iterator)) {
    const CassFunctionMeta *meta = cass_iterator_get_function_meta(iterator);
    zval zfunction = php_driver_create_function(self->schema, meta);

    if (!Z_ISUNDEF(zfunction)) {
      php_driver_function *function = PHP_DRIVER_GET_FUNCTION(&(zfunction));

      if (Z_TYPE(function->signature) == IS_STRING) {
        add_assoc_zval_ex(return_value, Z_STRVAL(function->signature), Z_STRLEN(function->signature), &(zfunction));
      } else {
        add_next_index_zval(return_value, &(zfunction));
      }
    }
  }

  cass_iterator_free(iterator);
}

static zval
php_driver_create_aggregate(php_driver_ref* schema,
                               const CassAggregateMeta *meta)
{
  zval result;
  php_driver_aggregate *aggregate;
  const char *full_name;
  size_t full_name_length;

  ZVAL_UNDEF(&(result));


  object_init_ex(&(result), php_driver_default_aggregate_ce);

  aggregate = PHP_DRIVER_GET_AGGREGATE(&(result));
  aggregate->schema = php_driver_add_ref(schema);
  aggregate->meta   = meta;

  cass_aggregate_meta_full_name(aggregate->meta, &full_name, &full_name_length);

  ZVAL_STRINGL(&(aggregate->signature), full_name, full_name_length);

  return result;
}

PHP_METHOD(DefaultKeyspace, aggregate)
{
  php_driver_keyspace *self;
  char *name;
  size_t name_len;
  zval *args = NULL;
  smart_str arguments = {0};
  int argc = 0;
  const CassAggregateMeta *meta = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|*",
                            &name, &name_len,
                            &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_KEYSPACE(getThis());

  if (argc > 0) {
    if (php_driver_arguments_string(args, argc, &arguments) == FAILURE) {

      return;
    }
  }

  meta =
      cass_keyspace_meta_aggregate_by_name_n(self->meta,
                                             name, name_len,
                                             CASS_SMART_STR_VAL(arguments),
                                             CASS_SMART_STR_LEN(arguments));
  if (meta) {
    zval zaggregate = php_driver_create_aggregate(self->schema, meta);
    RETVAL_ZVAL(&(zaggregate), 1, 1);
  } else {
    RETVAL_FALSE;
  }

  smart_str_free(&arguments);

}

PHP_METHOD(DefaultKeyspace, aggregates)
{
  php_driver_keyspace *self;
  CassIterator *iterator;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self     = PHP_DRIVER_GET_KEYSPACE(getThis());
  iterator = cass_iterator_aggregates_from_keyspace_meta(self->meta);

  array_init(return_value);
  while (cass_iterator_next(iterator)) {
    const CassAggregateMeta *meta = cass_iterator_get_aggregate_meta(iterator);
    zval zaggregate = php_driver_create_aggregate(self->schema, meta);

    if (!Z_ISUNDEF(zaggregate)) {
      php_driver_aggregate *aggregate = PHP_DRIVER_GET_AGGREGATE(&(zaggregate));

      if (Z_TYPE(aggregate->signature) == IS_STRING) {
        add_assoc_zval_ex(return_value, Z_STRVAL(aggregate->signature), Z_STRLEN(aggregate->signature), &(zaggregate));
      } else {
        add_next_index_zval(return_value, &(zaggregate));
      }
    }
  }

  cass_iterator_free(iterator);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_name, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_signature, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, name)
  ZEND_ARG_INFO(0, ...)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_default_keyspace_methods[] = {
  PHP_ME(DefaultKeyspace, name, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, replicationClassName, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, replicationOptions, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, hasDurableWrites, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, table, arginfo_name, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, tables, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, userType, arginfo_name, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, userTypes, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, materializedView, arginfo_name, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, materializedViews, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, function, arginfo_signature, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, functions, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, aggregate, arginfo_signature, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultKeyspace, aggregates, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_default_keyspace_handlers;

static HashTable *
php_driver_type_default_keyspace_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static int
php_driver_default_keyspace_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_default_keyspace_free(zend_object *object)
{
  php_driver_keyspace *self = php_driver_keyspace_object_fetch(object);;

  if (self->schema) {
    php_driver_del_ref(&self->schema);
    self->schema = NULL;
  }
  self->meta = NULL;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_default_keyspace_new(zend_class_entry *ce)
{
  php_driver_keyspace *self =
      CASS_ZEND_OBJECT_ECALLOC(keyspace, ce);

  self->meta   = NULL;
  self->schema = NULL;

  CASS_ZEND_OBJECT_INIT_EX(keyspace, default_keyspace, self, ce);
}

void php_driver_define_DefaultKeyspace()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\DefaultKeyspace", php_driver_default_keyspace_methods);
  php_driver_default_keyspace_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_default_keyspace_ce, 1, php_driver_keyspace_ce);
  php_driver_default_keyspace_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_default_keyspace_ce->create_object = php_driver_default_keyspace_new;

  memcpy(&php_driver_default_keyspace_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_default_keyspace_handlers.get_gc          = php_driver_type_default_keyspace_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_default_keyspace_handlers, php_driver_default_keyspace_compare);
  php_driver_default_keyspace_handlers.clone_obj = NULL;
}
