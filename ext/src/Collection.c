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
#include "src/Collection.h"

zend_class_entry *php_driver_collection_ce = NULL;

void
php_driver_collection_add(php_driver_collection *collection, zval *object)
{
  zend_hash_next_index_insert(&collection->values, object);
  Z_TRY_ADDREF_P(object);
  collection->dirty = 1;
}

static int
php_driver_collection_del(php_driver_collection *collection, unsigned long index)
{
  if (zend_hash_index_del(&collection->values, index) == SUCCESS) {
    collection->dirty = 1;
    return 1;
  }

  return 0;
}

static int
php_driver_collection_get(php_driver_collection *collection, unsigned long index, zval *zvalue)
{
  zval *value;
  if (CASS_ZEND_HASH_INDEX_FIND(&collection->values, index, value)) {
    *zvalue = *value;
    return 1;
  }
  return 0;
}

static int
php_driver_collection_find(php_driver_collection *collection, zval *object, long *index)
{
  zend_ulong num_key;
  zval *current;
  ZEND_HASH_FOREACH_NUM_KEY_VAL(&collection->values, num_key, current) {
    zval compare;
    is_equal_function(&compare, object, current);
    if (Z_TYPE_P(&compare) == IS_TRUE) {
      *index = (long) num_key;
      return 1;
    }
  } ZEND_HASH_FOREACH_END();

  return 0;
}

static void
php_driver_collection_populate(php_driver_collection *collection, zval *array)
{
  zval *current;
  ZEND_HASH_FOREACH_VAL(&collection->values, current) {
    if (add_next_index_zval(array, current) == SUCCESS)
      Z_TRY_ADDREF_P(current);
    else
      break;
  } ZEND_HASH_FOREACH_END();
}

/* {{{ Collection::__construct(type) */
PHP_METHOD(Collection, __construct)
{
  php_driver_collection *self;
  zval *type;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &type) == FAILURE)
    return;

  self = PHP_DRIVER_GET_COLLECTION(getThis());

  if (Z_TYPE_P(type) == IS_STRING) {
    CassValueType value_type;
    if (!php_driver_value_type(Z_STRVAL_P(type), &value_type))
      return;
    self->type = php_driver_type_collection_from_value_type(value_type);
  } else if (Z_TYPE_P(type) == IS_OBJECT &&
             instanceof_function(Z_OBJCE_P(type), php_driver_type_ce)) {
    if (!php_driver_type_validate(type, "type")) {
      return;
    }
    self->type = php_driver_type_collection(type);
    Z_ADDREF_P(type);
  } else {
    INVALID_ARGUMENT(type, "a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
  }
}
/* }}} */

/* {{{ Collection::type() */
PHP_METHOD(Collection, type)
{
  php_driver_collection *self = PHP_DRIVER_GET_COLLECTION(getThis());
  RETURN_ZVAL(&(self->type), 1, 0);
}

/* {{{ Collection::values() */
PHP_METHOD(Collection, values)
{
  php_driver_collection *collection = NULL;
  array_init(return_value);
  collection = PHP_DRIVER_GET_COLLECTION(getThis());
  php_driver_collection_populate(collection, return_value);
}
/* }}} */

/* {{{ Collection::add(mixed) */
PHP_METHOD(Collection, add)
{
  php_driver_collection *self = NULL;
  zval *args = NULL;
  int argc = 0, i;
  php_driver_type *type;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE)
    return;

  self = PHP_DRIVER_GET_COLLECTION(getThis());
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  for (i = 0; i < argc; i++) {
    if (Z_TYPE_P(&(args[i])) == IS_NULL) {

      zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                              "Invalid value: null is not supported inside collections");
      RETURN_FALSE;
    }

    if (!php_driver_validate_object(&(args[i]),
                                    &(type->data.collection.value_type))) {

      RETURN_FALSE;
    }
  }

  for (i = 0; i < argc; i++) {
    php_driver_collection_add(self, &(args[i]));
  }


  RETVAL_LONG(zend_hash_num_elements(&self->values));
}
/* }}} */

/* {{{ Collection::get(int) */
PHP_METHOD(Collection, get)
{
  long key;
  php_driver_collection *self = NULL;
  zval value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_COLLECTION(getThis());

  if (php_driver_collection_get(self, (unsigned long) key, &value))
    RETURN_ZVAL(&(value), 1, 0);
}
/* }}} */

/* {{{ Collection::find(mixed) */
PHP_METHOD(Collection, find)
{
  zval *object;
  php_driver_collection *collection = NULL;
  long index;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &object) == FAILURE)
    return;

  collection = PHP_DRIVER_GET_COLLECTION(getThis());

  if (php_driver_collection_find(collection, object, &index))
    RETURN_LONG(index);
}
/* }}} */

/* {{{ Collection::count() */
PHP_METHOD(Collection, count)
{
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());
  RETURN_LONG(zend_hash_num_elements(&collection->values));
}
/* }}} */

/* {{{ Collection::current() */
PHP_METHOD(Collection, current)
{
  zval *current;
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());

  if (CASS_ZEND_HASH_GET_CURRENT_DATA(&collection->values, current)) {
    RETURN_ZVAL(current, 1, 0);
  }
}
/* }}} */

/* {{{ Collection::key() */
PHP_METHOD(Collection, key)
{
  zend_ulong num_key;
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());
  if (zend_hash_get_current_key(&collection->values, NULL, &num_key) == HASH_KEY_IS_LONG) {
    RETURN_LONG(num_key);
  }
}
/* }}} */

/* {{{ Collection::next() */
PHP_METHOD(Collection, next)
{
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());
  zend_hash_move_forward(&collection->values);
}
/* }}} */

/* {{{ Collection::valid() */
PHP_METHOD(Collection, valid)
{
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());
  RETURN_BOOL(zend_hash_has_more_elements(&collection->values) == SUCCESS);
}
/* }}} */

/* {{{ Collection::rewind() */
PHP_METHOD(Collection, rewind)
{
  php_driver_collection *collection = PHP_DRIVER_GET_COLLECTION(getThis());
  zend_hash_internal_pointer_reset(&collection->values);
}
/* }}} */

/* {{{ Collection::remove(key) */
PHP_METHOD(Collection, remove)
{
  long index;
  php_driver_collection *collection = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &index) == FAILURE) {
    return;
  }

  collection = PHP_DRIVER_GET_COLLECTION(getThis());

  if (php_driver_collection_del(collection, (unsigned long) index)) {
    RETURN_TRUE;
  }

  RETURN_FALSE;
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_value, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_index, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_collection_methods[] = {
  PHP_ME(Collection, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Collection, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, values, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, add, arginfo_value, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, get, arginfo_index, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, find, arginfo_value, ZEND_ACC_PUBLIC)
  /* Countable */
  PHP_ME(Collection, count, arginfo_none, ZEND_ACC_PUBLIC)
  /* Iterator */
  PHP_ME(Collection, current, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, key, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, next, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, valid, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, rewind, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Collection, remove, arginfo_index, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_collection_handlers;

static HashTable *
php_driver_collection_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_collection_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval values;

  php_driver_collection  *self = CASS_COMPAT_GET_COLLECTION(object);
  HashTable             *props = zend_std_get_properties(object);

  zend_hash_str_update(props, "type", strlen("type"), &(self->type));
  Z_ADDREF_P(&(self->type));


  array_init(&(values));
  php_driver_collection_populate(self, &(values));
  zend_hash_str_update(props, "values", strlen("values"), &(values));

  return props;
}

static int
php_driver_collection_compare(zval *obj1, zval *obj2)
{
  HashPosition pos1;
  HashPosition pos2;
  zval *current1;
  zval *current2;
  php_driver_collection *collection1;
  php_driver_collection *collection2;
  php_driver_type *type1;
  php_driver_type *type2;
  int result;

  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  collection1 = PHP_DRIVER_GET_COLLECTION(obj1);
  collection2 = PHP_DRIVER_GET_COLLECTION(obj2);

  type1 = PHP_DRIVER_GET_TYPE(&(collection1->type));
  type2 = PHP_DRIVER_GET_TYPE(&(collection2->type));

  result = php_driver_type_compare(type1, type2);
  if (result != 0) return result;

  if (zend_hash_num_elements(&collection1->values) != zend_hash_num_elements(&collection2->values)) {
    return zend_hash_num_elements(&collection1->values) < zend_hash_num_elements(&collection2->values) ? -1 : 1;
  }

  zend_hash_internal_pointer_reset_ex(&collection1->values, &pos1);
  zend_hash_internal_pointer_reset_ex(&collection2->values, &pos2);

  while (CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&collection1->values, current1, &pos1) &&
         CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&collection2->values, current2, &pos2)) {
    result = php_driver_value_compare(current1, current2);
    if (result != 0) return result;
    zend_hash_move_forward_ex(&collection1->values, &pos1);
    zend_hash_move_forward_ex(&collection2->values, &pos2);
  }

  return 0;
}

static unsigned
php_driver_collection_hash_value(zval *obj)
{
  zval *current;
  unsigned hashv = 0;
  php_driver_collection *self = PHP_DRIVER_GET_COLLECTION(obj);

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
php_driver_collection_free(zend_object *object)
{
  php_driver_collection *self =
          php_driver_collection_object_fetch(object);;

  zend_hash_destroy(&self->values);
  CASS_ZVAL_MAYBE_DESTROY(self->type);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_collection_new(zend_class_entry *ce)
{
  php_driver_collection *self =
      CASS_ZEND_OBJECT_ECALLOC(collection, ce);

  zend_hash_init(&self->values, 0, NULL, ZVAL_PTR_DTOR, 0);
  self->dirty = 1;
  ZVAL_UNDEF(&(self->type));

  CASS_ZEND_OBJECT_INIT(collection, self, ce);
}

void php_driver_define_Collection()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Collection", php_driver_collection_methods);
  php_driver_collection_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_collection_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_collection_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_collection_handlers.std.get_properties  = php_driver_collection_properties;
  php_driver_collection_handlers.std.get_gc          = php_driver_collection_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_collection_handlers.std, php_driver_collection_compare);
  php_driver_collection_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_collection_ce->create_object = php_driver_collection_new;
  zend_class_implements(php_driver_collection_ce, 2, spl_ce_Countable, zend_ce_iterator);

  php_driver_collection_handlers.hash_value = php_driver_collection_hash_value;
  php_driver_collection_handlers.std.clone_obj = NULL;
}
