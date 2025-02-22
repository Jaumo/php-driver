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
#include "result.h"
#include "math.h"
#include "collections.h"
#include "types.h"
#include "src/Collection.h"
#include "src/Map.h"
#include "src/Set.h"
#include "src/Tuple.h"
#include "src/UserTypeValue.h"

int
php_driver_value(const CassValue* value, const CassDataType* data_type, zval *out)
{
  const char *v_string;
  size_t v_string_len;
  const cass_byte_t *v_bytes;
  size_t v_bytes_len;
  const cass_byte_t *v_decimal;
  size_t v_decimal_len;
  cass_int32_t v_decimal_scale;
  cass_int32_t v_int_32;
  cass_bool_t v_boolean;
  cass_double_t v_double;
  php_driver_uuid *uuid;
  CassIterator *iterator;
  php_driver_numeric *numeric = NULL;
  php_driver_timestamp *timestamp = NULL;
  php_driver_date *date = NULL;
  php_driver_time *time = NULL;
  php_driver_blob *blob = NULL;
  php_driver_inet *inet = NULL;
  php_driver_duration *duration = NULL;
  php_driver_collection *collection = NULL;
  php_driver_map *map = NULL;
  php_driver_set *set = NULL;
  php_driver_tuple *tuple = NULL;
  php_driver_user_type_value *user_type_value = NULL;
  unsigned long index;

  CassValueType type = cass_data_type_type(data_type);
  const CassDataType* primary_type;
  const CassDataType* secondary_type;



  if (cass_value_is_null(value)) {
    ZVAL_NULL(out);
    return SUCCESS;
  }

  switch (type) {
  case CASS_VALUE_TYPE_ASCII:
  case CASS_VALUE_TYPE_TEXT:
  case CASS_VALUE_TYPE_VARCHAR:
    ASSERT_SUCCESS_BLOCK(cass_value_get_string(value, &v_string, &v_string_len),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    ZVAL_STRINGL(out, v_string, v_string_len);
    break;
  case CASS_VALUE_TYPE_INT:
    ASSERT_SUCCESS_BLOCK(cass_value_get_int32(value, &v_int_32),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    ZVAL_LONG(out, v_int_32);
    break;
  case CASS_VALUE_TYPE_COUNTER:
  case CASS_VALUE_TYPE_BIGINT:
    object_init_ex(out, php_driver_bigint_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_int64(value, &numeric->data.bigint.value),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_SMALL_INT:
    object_init_ex(out, php_driver_smallint_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_int16(value, &numeric->data.smallint.value),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_TINY_INT:
    object_init_ex(out, php_driver_tinyint_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_int8(value, &numeric->data.tinyint.value),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_TIMESTAMP:
    object_init_ex(out, php_driver_timestamp_ce);
    timestamp = PHP_DRIVER_GET_TIMESTAMP(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_int64(value, &timestamp->timestamp),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_DATE:
    object_init_ex(out, php_driver_date_ce);
    date = PHP_DRIVER_GET_DATE(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_uint32(value, &date->date),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_TIME:
    object_init_ex(out, php_driver_time_ce);
    time = PHP_DRIVER_GET_TIME(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_int64(value, &time->time),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_BLOB:
    object_init_ex(out, php_driver_blob_ce);
    blob = PHP_DRIVER_GET_BLOB(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_bytes(value, &v_bytes, &v_bytes_len),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    blob->data = emalloc(v_bytes_len * sizeof(cass_byte_t));
    blob->size = v_bytes_len;
    memcpy(blob->data, v_bytes, v_bytes_len);
    break;
  case CASS_VALUE_TYPE_VARINT:
    object_init_ex(out, php_driver_varint_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_bytes(value, &v_bytes, &v_bytes_len),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    import_twos_complement((cass_byte_t*) v_bytes, v_bytes_len, &numeric->data.varint.value);
    break;
  case CASS_VALUE_TYPE_UUID:
    object_init_ex(out, php_driver_uuid_ce);
    uuid = PHP_DRIVER_GET_UUID(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_uuid(value, &uuid->uuid),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_TIMEUUID:
    object_init_ex(out, php_driver_timeuuid_ce);
    uuid = PHP_DRIVER_GET_UUID(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_uuid(value, &uuid->uuid),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_BOOLEAN:
    ASSERT_SUCCESS_BLOCK(cass_value_get_bool(value, &v_boolean),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    if (v_boolean) {
      ZVAL_TRUE(out);
    } else {
      ZVAL_FALSE(out);
    }
    break;
  case CASS_VALUE_TYPE_INET:
    object_init_ex(out, php_driver_inet_ce);
    inet = PHP_DRIVER_GET_INET(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_inet(value, &inet->inet),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_DECIMAL:
    object_init_ex(out, php_driver_decimal_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_decimal(value, &v_decimal, &v_decimal_len, &v_decimal_scale),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    import_twos_complement((cass_byte_t*) v_decimal, v_decimal_len, &numeric->data.decimal.value);
    numeric->data.decimal.scale = v_decimal_scale;
    break;
  case CASS_VALUE_TYPE_DURATION:
    object_init_ex(out, php_driver_duration_ce);
    duration = PHP_DRIVER_GET_DURATION(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_duration(value, &duration->months, &duration->days, &duration->nanos),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    break;
  case CASS_VALUE_TYPE_DOUBLE:
    ASSERT_SUCCESS_BLOCK(cass_value_get_double(value, &v_double),
      zval_ptr_dtor(out);
      return FAILURE;
    );
    ZVAL_DOUBLE(out, v_double);
    break;
  case CASS_VALUE_TYPE_FLOAT:
    object_init_ex(out, php_driver_float_ce);
    numeric = PHP_DRIVER_GET_NUMERIC(out);
    ASSERT_SUCCESS_BLOCK(cass_value_get_float(value, &numeric->data.floating.value),
      zval_ptr_dtor(out);
      return FAILURE;
    )
    break;
  case CASS_VALUE_TYPE_LIST:
    object_init_ex(out, php_driver_collection_ce);
    collection = PHP_DRIVER_GET_COLLECTION(out);

    primary_type = cass_data_type_sub_data_type(data_type, 0);
    collection->type = php_driver_type_from_data_type(data_type);

    iterator = cass_iterator_from_collection(value);

    while (cass_iterator_next(iterator)) {
      zval v;

      if (php_driver_value(cass_iterator_get_value(iterator), primary_type, &v) == FAILURE) {
        cass_iterator_free(iterator);
        zval_ptr_dtor(out);
        return FAILURE;
      }

      php_driver_collection_add(collection, &(v));
      zval_ptr_dtor(&v);
    }

    cass_iterator_free(iterator);
    break;
  case CASS_VALUE_TYPE_MAP:
    object_init_ex(out, php_driver_map_ce);
    map = PHP_DRIVER_GET_MAP(out);

    primary_type = cass_data_type_sub_data_type(data_type, 0);
    secondary_type = cass_data_type_sub_data_type(data_type, 1);
    map->type = php_driver_type_from_data_type(data_type);

    iterator = cass_iterator_from_map(value);

    while (cass_iterator_next(iterator)) {
      zval k;
      zval v;

      if (php_driver_value(cass_iterator_get_map_key(iterator), primary_type, &k) == FAILURE ||
          php_driver_value(cass_iterator_get_map_value(iterator), secondary_type, &v) == FAILURE) {
        cass_iterator_free(iterator);
        zval_ptr_dtor(out);
        return FAILURE;
      }

      php_driver_map_set(map, &(k), &(v));
      zval_ptr_dtor(&k);
      zval_ptr_dtor(&v);
    }

    cass_iterator_free(iterator);
    break;
  case CASS_VALUE_TYPE_SET:
    object_init_ex(out, php_driver_set_ce);
    set = PHP_DRIVER_GET_SET(out);

    primary_type = cass_data_type_sub_data_type(data_type, 0);
    set->type = php_driver_type_from_data_type(data_type);

    iterator = cass_iterator_from_collection(value);

    while (cass_iterator_next(iterator)) {
      zval v;

      if (php_driver_value(cass_iterator_get_value(iterator), primary_type, &v) == FAILURE) {
        cass_iterator_free(iterator);
        zval_ptr_dtor(out);
        return FAILURE;
      }

      php_driver_set_add(set, &(v));
      zval_ptr_dtor(&v);
    }

    cass_iterator_free(iterator);
    break;
  case CASS_VALUE_TYPE_TUPLE:
    object_init_ex(out, php_driver_tuple_ce);
    tuple = PHP_DRIVER_GET_TUPLE(out);

    tuple->type = php_driver_type_from_data_type(data_type);

    iterator = cass_iterator_from_tuple(value);

    index = 0;
    while (cass_iterator_next(iterator)) {
      const CassValue* value = cass_iterator_get_value(iterator);

      if (!cass_value_is_null(value)) {
        zval v;

        primary_type = cass_data_type_sub_data_type(data_type, index);
        if (php_driver_value(value, primary_type, &v) == FAILURE) {
          cass_iterator_free(iterator);
          zval_ptr_dtor(out);
          return FAILURE;
        }

        php_driver_tuple_set(tuple, index, &(v));
        zval_ptr_dtor(&v);
      }

      index++;
    }

    cass_iterator_free(iterator);
    break;
  case CASS_VALUE_TYPE_UDT:
    object_init_ex(out, php_driver_user_type_value_ce);
    user_type_value = PHP_DRIVER_GET_USER_TYPE_VALUE(out);

    user_type_value->type = php_driver_type_from_data_type(data_type);

    iterator = cass_iterator_fields_from_user_type(value);

    index = 0;
    while (cass_iterator_next(iterator)) {
      const CassValue* value = cass_iterator_get_user_type_field_value(iterator);

      if (!cass_value_is_null(value)) {
        const char *name;
        size_t name_length;
        zval v;

        primary_type = cass_data_type_sub_data_type(data_type, index);
        if (php_driver_value(value, primary_type, &v) == FAILURE) {
          cass_iterator_free(iterator);
          zval_ptr_dtor(out);
          return FAILURE;
        }

        cass_iterator_get_user_type_field_name(iterator, &name, &name_length);
        php_driver_user_type_value_set(user_type_value,
                                          name, name_length,
                                          &(v));
        zval_ptr_dtor(&v);
      }

      index++;
    }

    cass_iterator_free(iterator);
    break;
  default:
    ZVAL_NULL(out);
    break;
  }

  return SUCCESS;
}

int
php_driver_get_keyspace_field(const CassKeyspaceMeta *metadata, const char *field_name, zval *out)
{
  const CassValue *value;

  value = cass_keyspace_meta_field_by_name(metadata, field_name);

  if (value == NULL || cass_value_is_null(value)) {

    ZVAL_NULL(out);
    return SUCCESS;
  }

  return php_driver_value(value, cass_value_data_type(value), out);
}

int
php_driver_get_table_field(const CassTableMeta *metadata, const char *field_name, zval *out)
{
  const CassValue *value;

  value = cass_table_meta_field_by_name(metadata, field_name);

  if (value == NULL || cass_value_is_null(value)) {

    ZVAL_NULL(out);
    return SUCCESS;
  }

  return php_driver_value(value, cass_value_data_type(value), out);
}

int
php_driver_get_column_field(const CassColumnMeta *metadata, const char *field_name, zval *out)
{
  const CassValue *value;

  value = cass_column_meta_field_by_name(metadata, field_name);

  if (value == NULL || cass_value_is_null(value)) {

    ZVAL_NULL(out);
    return SUCCESS;
  }

  return php_driver_value(value, cass_value_data_type(value), out);
}

int
php_driver_get_result(const CassResult *result, zval *out)
{
  zval     rows;
  zval     row;
  const CassRow   *cass_row;
  const char      *column_name;
  size_t           column_name_len;
  const CassDataType* column_type;
  const CassValue *column_value;
  CassIterator    *iterator = NULL;
  size_t           columns = -1;
  char           **column_names;
  unsigned         i;


  array_init(&(rows));

  iterator = cass_iterator_from_result(result);
  columns  = cass_result_column_count(result);

  column_names = (char**) ecalloc(columns, sizeof(char*));

  while (cass_iterator_next(iterator)) {

    array_init(&(row));
    cass_row = cass_iterator_get_row(iterator);

    for (i = 0; i < columns; i++) {
      zval value;

      if (column_names[i] == NULL) {
        cass_result_column_name(result, i, &column_name, &column_name_len);
        column_names[i] = estrndup(column_name, column_name_len);
      }

      column_type  = cass_result_column_data_type(result, i);
      column_value = cass_row_get_column(cass_row, i);

      if (php_driver_value(column_value, column_type, &value) == FAILURE) {
        zval_ptr_dtor(&row);
        zval_ptr_dtor(&rows);

        for (i = 0; i < columns; i++) {
          if (column_names[i]) {
            efree(column_names[i]);
          }
        }

        efree(column_names);
        cass_iterator_free(iterator);

        return FAILURE;
      }

      add_assoc_zval_ex(&(row), column_names[i], strlen(column_names[i]), &(value));
    }

    add_next_index_zval(&(rows),
                        &(row));
  }

  for (i = 0; i < columns; i++) {
    if (column_names[i] != NULL) {
      efree(column_names[i]);
    }
  }

  efree(column_names);
  cass_iterator_free(iterator);

  *out = rows;

  return SUCCESS;
}
