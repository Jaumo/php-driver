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

#ifndef PHP_DRIVER_DEFAULT_COLUMN_H
#define PHP_DRIVER_DEFAULT_COLUMN_H

#include "php_driver.h"

php5to7_zval
php_driver_create_column(php_driver_ref* schema,
                            const CassColumnMeta *meta);

#endif /* PHP_DRIVER_DEFAULT_COLUMN_H */
