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
#include "Map.h"

zend_class_entry *php_driver_map_ce = NULL;

int
php_driver_map_set(php_driver_map *map, zval *zkey, zval *zvalue)
{
  php_driver_map_entry *entry;
  php_driver_type *type;

  if (Z_TYPE_P(zkey) == IS_NULL) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Invalid key: null is not supported inside maps");
    return 0;
  }

  if (Z_TYPE_P(zvalue) == IS_NULL) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Invalid value: null is not supported inside maps");
    return 0;
  }

  type = PHP_DRIVER_GET_TYPE(&(map->type));

  if (!php_driver_validate_object(zkey, &(type->data.map.key_type))) {
    return 0;
  }

  if (!php_driver_validate_object(zvalue, &(type->data.map.value_type))) {
    return 0;
  }

  map->dirty = 1;
  HASH_FIND_ZVAL(map->entries, zkey, entry);
  if (entry == NULL) {
    entry = (php_driver_map_entry *) emalloc(sizeof(php_driver_map_entry));
    ZVAL_COPY(&(entry->key), zkey);
    ZVAL_COPY(&(entry->value), zvalue);
    HASH_ADD_ZVAL(map->entries, key, entry);
  } else {
    zval prev_value = entry->value;
    ZVAL_COPY(&(entry->value), zvalue);
    zval_ptr_dtor(&prev_value);
  }

  return 1;
}

static int
php_driver_map_get(php_driver_map *map, zval *zkey, zval *zvalue)
{
  php_driver_map_entry *entry;
  php_driver_type *type;
  int result = 0;

  type = PHP_DRIVER_GET_TYPE(&(map->type));

  if (!php_driver_validate_object(zkey, &(type->data.map.key_type))) {
    return 0;
  }

  HASH_FIND_ZVAL(map->entries, zkey, entry);
  if (entry != NULL) {
    *zvalue = entry->value;
    result = 1;
  }

  return result;
}

static int
php_driver_map_del(php_driver_map *map, zval *zkey)
{
  php_driver_map_entry *entry;
  php_driver_type *type;
  int result = 0;

  type = PHP_DRIVER_GET_TYPE(&(map->type));

  if (!php_driver_validate_object(zkey, &(type->data.map.key_type))) {
    return 0;
  }

  HASH_FIND_ZVAL(map->entries, zkey, entry);
  if (entry != NULL) {
    map->dirty = 1;
    if (entry == map->iter_temp) {
      map->iter_temp = (php_driver_map_entry *)map->iter_temp->hh.next;
    }
    HASH_DEL(map->entries, entry);
    zval_ptr_dtor(&entry->key);
    zval_ptr_dtor(&entry->value);
    efree(entry);
    result = 1;
  }

  return result;
}

static int
php_driver_map_has(php_driver_map *map, zval *zkey)
{
  php_driver_map_entry *entry;
  php_driver_type *type;
  int result = 0;

  type = PHP_DRIVER_GET_TYPE(&(map->type));

  if (!php_driver_validate_object(zkey, &(type->data.map.key_type))) {
    return 0;
  }

  HASH_FIND_ZVAL(map->entries, zkey, entry);
  if (entry != NULL) {
    result = 1;
  }

  return result;
}

static void
php_driver_map_populate_keys(const php_driver_map *map, zval *array)
{
  php_driver_map_entry *curr,  *temp;
  HASH_ITER(hh, map->entries, curr, temp) {
    if (add_next_index_zval(array, &(curr->key)) != SUCCESS) {
      break;
    }
    Z_TRY_ADDREF_P(&(curr->key));
  }
}

static void
php_driver_map_populate_values(const php_driver_map *map, zval *array)
{
  php_driver_map_entry *curr, *temp;
  HASH_ITER(hh, map->entries, curr, temp) {
    if (add_next_index_zval(array, &(curr->value)) != SUCCESS) {
      break;
    }
    Z_TRY_ADDREF_P(&(curr->value));
  }
}

/* {{{ Map::__construct(type, type) */
PHP_METHOD(Map, __construct)
{
  php_driver_map *self;
  zval *key_type;
  zval *value_type;
  zval scalar_key_type;
  zval scalar_value_type;

  ZVAL_UNDEF(&(scalar_key_type));
  ZVAL_UNDEF(&(scalar_value_type));

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &key_type, &value_type) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (Z_TYPE_P(key_type) == IS_STRING) {
    CassValueType type;
    if (!php_driver_value_type(Z_STRVAL_P(key_type), &type))
      return;
    scalar_key_type = php_driver_type_scalar(type);
    key_type = &(scalar_key_type);
  } else if (Z_TYPE_P(key_type) == IS_OBJECT &&
             instanceof_function(Z_OBJCE_P(key_type), php_driver_type_ce)) {
    if (!php_driver_type_validate(key_type, "keyType")) {
      return;
    }
    Z_ADDREF_P(key_type);
  } else {
    throw_invalid_argument(key_type,
                           "keyType",
                           "a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
    return;
  }

  if (Z_TYPE_P(value_type) == IS_STRING) {
    CassValueType type;
    if (!php_driver_value_type(Z_STRVAL_P(value_type), &type))
      return;
    scalar_value_type = php_driver_type_scalar(type);
    value_type = &(scalar_value_type);
  } else if (Z_TYPE_P(value_type) == IS_OBJECT &&
             instanceof_function(Z_OBJCE_P(value_type), php_driver_type_ce)) {
    if (!php_driver_type_validate(value_type, "valueType")) {
      return;
    }
    Z_ADDREF_P(value_type);
  } else {
    zval_ptr_dtor(key_type);
    throw_invalid_argument(value_type,
                           "valueType",
                           "a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
    return;
  }

  self->type = php_driver_type_map(key_type, value_type);
}
/* }}} */

/* {{{ Map::type() */
PHP_METHOD(Map, type)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  RETURN_ZVAL(&(self->type), 1, 0);
}
/* }}} */

PHP_METHOD(Map, keys)
{
  php_driver_map *self = NULL;
  array_init(return_value);
  self = PHP_DRIVER_GET_MAP(getThis());
  php_driver_map_populate_keys(self, return_value);
}

PHP_METHOD(Map, values)
{
  php_driver_map *self = NULL;
  array_init(return_value);
  self = PHP_DRIVER_GET_MAP(getThis());
  php_driver_map_populate_values(self, return_value);
}

PHP_METHOD(Map, set)
{
  zval *key;
  php_driver_map *self = NULL;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &key, &value) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_set(self, key, value))
    RETURN_TRUE;

  RETURN_FALSE;
}

PHP_METHOD(Map, get)
{
  zval *key;
  php_driver_map *self = NULL;
  zval value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_get(self, key, &value))
    RETURN_ZVAL(&(value), 1, 0);
}

PHP_METHOD(Map, remove)
{
  zval *key;
  php_driver_map *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_del(self, key))
    RETURN_TRUE;

  RETURN_FALSE;
}

PHP_METHOD(Map, has)
{
  zval *key;
  php_driver_map *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_has(self, key))
    RETURN_TRUE;

  RETURN_FALSE;
}

PHP_METHOD(Map, count)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  RETURN_LONG((long)HASH_COUNT(self->entries));
}

PHP_METHOD(Map, current)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  if (self->iter_curr != NULL)
    RETURN_ZVAL(&(self->iter_curr->value), 1, 0);
}

PHP_METHOD(Map, key)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  if (self->iter_curr != NULL)
    RETURN_ZVAL(&(self->iter_curr->key), 1, 0);
}

PHP_METHOD(Map, next)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  self->iter_curr = self->iter_temp;
  self->iter_temp = self->iter_temp != NULL ? (php_driver_map_entry *)self->iter_temp->hh.next : NULL;
}

PHP_METHOD(Map, valid)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  RETURN_BOOL(self->iter_curr != NULL);
}

PHP_METHOD(Map, rewind)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(getThis());
  self->iter_curr = self->entries;
  self->iter_temp = self->entries != NULL ? (php_driver_map_entry *)self->entries->hh.next : NULL;
}

PHP_METHOD(Map, offsetSet)
{
  zval *key;
  php_driver_map *self = NULL;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &key, &value) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  php_driver_map_set(self, key, value);
}

PHP_METHOD(Map, offsetGet)
{
  zval *key;
  php_driver_map *self = NULL;
  zval value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_get(self, key, &value))
    RETURN_ZVAL(&(value), 1, 0);
}

PHP_METHOD(Map, offsetUnset)
{
  zval *key;
  php_driver_map *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  php_driver_map_del(self, key);
}

PHP_METHOD(Map, offsetExists)
{
  zval *key;
  php_driver_map *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE)
    return;

  self = PHP_DRIVER_GET_MAP(getThis());

  if (php_driver_map_has(self, key))
    RETURN_TRUE;

  RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 2)
  ZEND_ARG_INFO(0, keyType)
  ZEND_ARG_INFO(0, valueType)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_two, 0, ZEND_RETURN_VALUE, 2)
  ZEND_ARG_INFO(0, key)
  ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_one, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, key)
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetExists, ZEND_RETURN_VALUE, 1, _IS_BOOL, 0)
  ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetGet, ZEND_RETURN_VALUE, 1, CASS_COMPAT_IS_MIXED, 0)
  ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetSet, ZEND_RETURN_VALUE, 2, IS_VOID, 0)
  ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
  ZEND_ARG_TYPE_INFO(0, value, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetUnset, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
  ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_map_methods[] = {
  PHP_ME(Map, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Map, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Map, keys, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Map, values, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Map, set, arginfo_two, ZEND_ACC_PUBLIC)
  PHP_ME(Map, get, arginfo_one, ZEND_ACC_PUBLIC)
  PHP_ME(Map, remove, arginfo_one, ZEND_ACC_PUBLIC)
  PHP_ME(Map, has, arginfo_one, ZEND_ACC_PUBLIC)
  /* Countable */
  PHP_ME(Map, count, arginfo_count, ZEND_ACC_PUBLIC)
  /* Iterator */
  PHP_ME(Map, current, arginfo_current, ZEND_ACC_PUBLIC)
  PHP_ME(Map, key, arginfo_key, ZEND_ACC_PUBLIC)
  PHP_ME(Map, next, arginfo_next, ZEND_ACC_PUBLIC)
  PHP_ME(Map, valid, arginfo_valid, ZEND_ACC_PUBLIC)
  PHP_ME(Map, rewind, arginfo_rewind, ZEND_ACC_PUBLIC)
  /* ArrayAccess */
  PHP_ME(Map, offsetSet, arginfo_offsetSet, ZEND_ACC_PUBLIC)
  PHP_ME(Map, offsetGet, arginfo_offsetGet, ZEND_ACC_PUBLIC)
  PHP_ME(Map, offsetUnset, arginfo_offsetUnset, ZEND_ACC_PUBLIC)
  PHP_ME(Map, offsetExists, arginfo_offsetExists, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_map_handlers;

static HashTable *
php_driver_map_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_map_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval keys;
  zval values;

  php_driver_map *self = CASS_COMPAT_GET_MAP(object);
  HashTable     *props = zend_std_get_properties(object);


  zend_hash_str_update(props, "type", strlen("type"), &(self->type));
  Z_ADDREF_P(&(self->type));


  array_init(&(keys));
  php_driver_map_populate_keys(self, &(keys));
  zend_hash_sort(Z_ARRVAL_P(&(keys)), php_driver_data_compare, 1);
  zend_hash_str_update(props, "keys", strlen("keys"), &(keys));


  array_init(&(values));
  php_driver_map_populate_values(self, &(values));
  zend_hash_sort(Z_ARRVAL_P(&(values)), php_driver_data_compare, 1);
  zend_hash_str_update(props, "values", strlen("values"), &(values));

  return props;
}

static int
php_driver_map_compare(zval *obj1, zval *obj2)
{
  php_driver_map_entry *curr, *temp;
  php_driver_map *map1;
  php_driver_map *map2;
  php_driver_type *type1;
  php_driver_type *type2;
  int result;

  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  map1 = PHP_DRIVER_GET_MAP(obj1);
  map2 = PHP_DRIVER_GET_MAP(obj2);

  type1 = PHP_DRIVER_GET_TYPE(&(map1->type));
  type2 = PHP_DRIVER_GET_TYPE(&(map2->type));

  result = php_driver_type_compare(type1, type2);
  if (result != 0) return result;

  if (HASH_COUNT(map1->entries) != HASH_COUNT(map2->entries)) {
   return HASH_COUNT(map1->entries) < HASH_COUNT(map2->entries) ? -1 : 1;
  }

  HASH_ITER(hh, map1->entries, curr, temp) {
    php_driver_map_entry *entry = NULL;
    HASH_FIND_ZVAL(map2->entries, &(curr->key), entry);
    if (entry == NULL) {
      return 1;
    }
    result = php_driver_value_compare(&(curr->value),
                                      &(entry->value));
    if (result != 0) return result;
  }

  return 0;
}

static unsigned
php_driver_map_hash_value(zval *obj)
{
  php_driver_map *self = PHP_DRIVER_GET_MAP(obj);
  php_driver_map_entry *curr, *temp;
  unsigned hashv = 0;

  if (!self->dirty) return self->hashv;

  HASH_ITER(hh, self->entries, curr, temp) {
    hashv = php_driver_combine_hash(hashv,
                                       php_driver_value_hash(&(curr->key)));
    hashv = php_driver_combine_hash(hashv,
                                       php_driver_value_hash(&(curr->value)));
  }

  self->hashv = hashv;
  self->dirty = 0;

  return hashv;
}

static void
php_driver_map_free(zend_object *object)
{
  php_driver_map *self = php_driver_map_object_fetch(object);;
  php_driver_map_entry *curr, *temp;

  HASH_ITER(hh, self->entries, curr, temp) {
    zval_ptr_dtor(&curr->key);
    zval_ptr_dtor(&curr->value);
    HASH_DEL(self->entries, curr);
    efree(curr);
  }

  CASS_ZVAL_MAYBE_DESTROY(self->type);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_map_new(zend_class_entry *ce)
{
  php_driver_map *self =
      CASS_ZEND_OBJECT_ECALLOC(map, ce);

  self->entries = self->iter_curr = self->iter_temp = NULL;
  self->dirty = 1;
  ZVAL_UNDEF(&(self->type));

  CASS_ZEND_OBJECT_INIT(map, self, ce);
}

void php_driver_define_Map()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Map", php_driver_map_methods);
  php_driver_map_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_map_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_map_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_map_handlers.std.get_properties  = php_driver_map_properties;
  php_driver_map_handlers.std.get_gc          = php_driver_map_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_map_handlers.std, php_driver_map_compare);
  php_driver_map_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_map_ce->create_object = php_driver_map_new;
  zend_class_implements(php_driver_map_ce, 3, zend_ce_countable, zend_ce_iterator, zend_ce_arrayaccess);

  php_driver_map_handlers.hash_value = php_driver_map_hash_value;
  php_driver_map_handlers.std.clone_obj = NULL;
}
