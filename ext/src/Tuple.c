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
#include "util/collections.h"
#include "util/hash.h"
#include "util/types.h"

#include "src/Type/Tuple.h"
#include "src/Tuple.h"

#include <zend_hash.h>

zend_class_entry *php_driver_tuple_ce = NULL;

void
php_driver_tuple_set(php_driver_tuple *tuple, unsigned long index, zval *object)
{
  zend_hash_index_update(&tuple->values, index, object);
  Z_TRY_ADDREF_P(object);
  tuple->dirty = 1;
}

static void
php_driver_tuple_populate(php_driver_tuple *tuple, zval *array)
{
  zend_ulong index;
  php_driver_type *type;
  zval *current;
  zval null;


  ZVAL_NULL(&(null));

  type = PHP_DRIVER_GET_TYPE(&(tuple->type));

  ZEND_HASH_FOREACH_NUM_KEY_VAL(&type->data.tuple.types, index, current) {
    zval *value = NULL;
    (void) current;
    if (CASS_ZEND_HASH_INDEX_FIND(&tuple->values, index, value)) {
      if (add_next_index_zval(array, value) == SUCCESS)
        Z_TRY_ADDREF_P(value);
      else
        break;
    } else {
      if (add_next_index_zval(array, &(null)) == SUCCESS)
        Z_TRY_ADDREF_P(&(null));
      else
        break;
    }
  } ZEND_HASH_FOREACH_END();

}

/* {{{ Tuple::__construct(types) */
PHP_METHOD(Tuple, __construct)
{
  php_driver_tuple *self;
  php_driver_type *type;
  HashTable *types;
  zval *current;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "h", &types) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TUPLE(getThis());
  self->type = php_driver_type_tuple();
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  ZEND_HASH_FOREACH_VAL(types, current) {
    zval *sub_type = current;
    zval scalar_type;

    if (Z_TYPE_P(sub_type) == IS_STRING) {
      CassValueType value_type;
      if (!php_driver_value_type(Z_STRVAL_P(sub_type), &value_type)) {
        return;
      }
      scalar_type = php_driver_type_scalar(value_type);
      if (!php_driver_type_tuple_add(type,
                                        &(scalar_type))) {
        return;
      }
    } else if (Z_TYPE_P(sub_type) == IS_OBJECT &&
               instanceof_function(Z_OBJCE_P(sub_type), php_driver_type_ce)) {
      if (!php_driver_type_validate(sub_type, "type")) {
        return;
      }
      if (php_driver_type_tuple_add(type,
                                        sub_type)) {
        Z_ADDREF_P(sub_type);
      } else {
        return;
      }
    } else {
      INVALID_ARGUMENT(sub_type, "a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
    }

  } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ Tuple::type() */
PHP_METHOD(Tuple, type)
{
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  RETURN_ZVAL(&(self->type), 1, 0);
}

/* {{{ Tuple::values() */
PHP_METHOD(Tuple, values)
{
  php_driver_tuple *self = NULL;
  array_init(return_value);
  self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_tuple_populate(self, return_value);
}
/* }}} */

/* {{{ Tuple::set(int, mixed) */
PHP_METHOD(Tuple, set)
{
  php_driver_tuple *self = NULL;
  long index;
  php_driver_type *type;
  zval *sub_type;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &index, &value) == FAILURE)
    return;

  self = PHP_DRIVER_GET_TUPLE(getThis());
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  if (index < 0 || index >= zend_hash_num_elements(&type->data.tuple.types)) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Index out of bounds");
    return;
  }

  if (!CASS_ZEND_HASH_INDEX_FIND(&type->data.tuple.types, index, sub_type) ||
      !php_driver_validate_object(value,
                                  sub_type)) {
    return;
  }

  php_driver_tuple_set(self, index, value);
}
/* }}} */

/* {{{ Tuple::get(int) */
PHP_METHOD(Tuple, get)
{
  php_driver_tuple *self = NULL;
  long index;
  php_driver_type *type;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &index) == FAILURE)
    return;

  self = PHP_DRIVER_GET_TUPLE(getThis());
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  if (index < 0 || index >= zend_hash_num_elements(&type->data.tuple.types)) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Index out of bounds");
    return;
  }

  if (CASS_ZEND_HASH_INDEX_FIND(&self->values, index, value)) {
    RETURN_ZVAL(value, 1, 0);
  }
}
/* }}} */

/* {{{ Tuple::count() */
PHP_METHOD(Tuple, count)
{
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));
  RETURN_LONG(zend_hash_num_elements(&type->data.tuple.types));
}
/* }}} */

/* {{{ Tuple::current() */
PHP_METHOD(Tuple, current)
{
  zend_ulong index;
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));

  if (zend_hash_get_current_key_ex(&type->data.tuple.types, ((void *) 0), &index, &self->pos) == HASH_KEY_IS_LONG) {
    zval *value;
    if (CASS_ZEND_HASH_INDEX_FIND(&self->values, index, value)) {
      RETURN_ZVAL(value, 1, 0);
    }
  }
}
/* }}} */

/* {{{ Tuple::key() */
PHP_METHOD(Tuple, key)
{
  zend_ulong index;
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));
  if (zend_hash_get_current_key_ex(&type->data.tuple.types, ((void *) 0), &index, &self->pos) == HASH_KEY_IS_LONG) {
    RETURN_LONG(index);
  }
}
/* }}} */

/* {{{ Tuple::next() */
PHP_METHOD(Tuple, next)
{
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));
  zend_hash_move_forward_ex(&type->data.tuple.types, &self->pos);
}
/* }}} */

/* {{{ Tuple::valid() */
PHP_METHOD(Tuple, valid)
{
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));
  RETURN_BOOL(zend_hash_has_more_elements_ex(&type->data.tuple.types, &self->pos) == SUCCESS);
}
/* }}} */

/* {{{ Tuple::rewind() */
PHP_METHOD(Tuple, rewind)
{
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(getThis());
  php_driver_type *type = PHP_DRIVER_GET_TYPE(&(self->type));
  zend_hash_internal_pointer_reset_ex(&type->data.tuple.types, &self->pos);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, types)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_value, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_index, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_count, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_current, ZEND_RETURN_VALUE, 0, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_key, ZEND_RETURN_VALUE, 0, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_next, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_valid, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_rewind, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_tuple_methods[] = {
  PHP_ME(Tuple, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, values, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, set, arginfo_value, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, get, arginfo_index, ZEND_ACC_PUBLIC)
  /* Countable */
  PHP_ME(Tuple, count, arginfo_count, ZEND_ACC_PUBLIC)
  /* Iterator */
  PHP_ME(Tuple, current, arginfo_current, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, key, arginfo_key, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, next, arginfo_next, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, valid, arginfo_valid, ZEND_ACC_PUBLIC)
  PHP_ME(Tuple, rewind, arginfo_rewind, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_tuple_handlers;

static HashTable *
php_driver_tuple_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_tuple_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval values;

  php_driver_tuple  *self = CASS_COMPAT_GET_TUPLE(object);
  HashTable             *props = zend_std_get_properties(object);

  zend_hash_str_update(props, "type", strlen("type"), &(self->type));
  Z_ADDREF_P(&(self->type));


  array_init(&(values));
  php_driver_tuple_populate(self, &(values));
  zend_hash_str_update(props, "values", strlen("values"), &(values));

  return props;
}

static int
php_driver_tuple_compare(zval *obj1, zval *obj2)
{
  HashPosition pos1;
  HashPosition pos2;
  zval *current1;
  zval *current2;
  php_driver_tuple *tuple1;
  php_driver_tuple *tuple2;
  php_driver_type *type1;
  php_driver_type *type2;
  int result;

  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  tuple1 = PHP_DRIVER_GET_TUPLE(obj1);
  tuple2 = PHP_DRIVER_GET_TUPLE(obj2);

  type1 = PHP_DRIVER_GET_TYPE(&(tuple1->type));
  type2 = PHP_DRIVER_GET_TYPE(&(tuple2->type));

  result = php_driver_type_compare(type1, type2);
  if (result != 0) return result;

  if (zend_hash_num_elements(&tuple1->values) != zend_hash_num_elements(&tuple2->values)) {
    return zend_hash_num_elements(&tuple1->values) < zend_hash_num_elements(&tuple2->values) ? -1 : 1;
  }

  zend_hash_internal_pointer_reset_ex(&tuple1->values, &pos1);
  zend_hash_internal_pointer_reset_ex(&tuple2->values, &pos2);

  while (CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&tuple1->values, current1, &pos1) &&
         CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&tuple2->values, current2, &pos2)) {
    result = php_driver_value_compare(current1,
                                         current2);
    if (result != 0) return result;
    zend_hash_move_forward_ex(&tuple1->values, &pos1);
    zend_hash_move_forward_ex(&tuple2->values, &pos2);
  }

  return 0;
}

static unsigned
php_driver_tuple_hash_value(zval *obj)
{
  zval *current;
  unsigned hashv = 0;
  php_driver_tuple *self = PHP_DRIVER_GET_TUPLE(obj);

  if (!self->dirty) return self->hashv;

  ZEND_HASH_FOREACH_VAL(&self->values, current) {
    hashv = php_driver_combine_hash(hashv,
                                       php_driver_value_hash(current));
  } ZEND_HASH_FOREACH_END();

  self->hashv = hashv;
  self->dirty = 0;

  return hashv;
}

static void
php_driver_tuple_free(zend_object *object)
{
  php_driver_tuple *self =
          php_driver_tuple_object_fetch(object);;

  zend_hash_destroy(&self->values);
  CASS_ZVAL_MAYBE_DESTROY(self->type);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_tuple_new(zend_class_entry *ce)
{
  php_driver_tuple *self =
      CASS_ZEND_OBJECT_ECALLOC(tuple, ce);

  zend_hash_init(&self->values, 0, NULL, ZVAL_PTR_DTOR, 0);
  self->pos = HT_INVALID_IDX;
  self->dirty = 1;
  ZVAL_UNDEF(&(self->type));

  CASS_ZEND_OBJECT_INIT(tuple, self, ce);
}

void php_driver_define_Tuple()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Tuple", php_driver_tuple_methods);
  php_driver_tuple_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_tuple_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_tuple_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_tuple_handlers.std.get_properties  = php_driver_tuple_properties;
  php_driver_tuple_handlers.std.get_gc          = php_driver_tuple_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_tuple_handlers.std, php_driver_tuple_compare);
  php_driver_tuple_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_tuple_ce->create_object = php_driver_tuple_new;
  zend_class_implements(php_driver_tuple_ce, 2, zend_ce_countable, zend_ce_iterator);

  php_driver_tuple_handlers.hash_value = php_driver_tuple_hash_value;
  php_driver_tuple_handlers.std.clone_obj = NULL;
}
