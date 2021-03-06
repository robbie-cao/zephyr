/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ZEPHYR__H
#define _ZEPHYR__H

#ifdef CONFIG_KERNEL_V2
#include <kernel.h>
#elif CONFIG_MICROKERNEL
#include <microkernel.h>
#elif CONFIG_NANOKERNEL
#include <nanokernel.h>
#else
#error "unknown kernel type!"
#endif

#ifdef CONFIG_MDEF
#include <sysgen.h>
#endif

#endif /* _ZEPHYR__H */
