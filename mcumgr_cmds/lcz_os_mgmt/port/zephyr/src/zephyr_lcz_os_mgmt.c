/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <zephyr.h>
#include <sys/reboot.h>
#include <debug/object_tracing.h>
#include <kernel_structs.h>
#include <mgmt/mgmt.h>
#include <util/mcumgr_util.h>
#include <lcz_os_mgmt/os_mgmt.h>
#include <lcz_os_mgmt/os_mgmt_impl.h>

static void zephyr_os_mgmt_reset_cb(struct k_timer *timer);
static void zephyr_os_mgmt_reset_work_handler(struct k_work *work);

static K_TIMER_DEFINE(zephyr_os_mgmt_reset_timer,
                      zephyr_os_mgmt_reset_cb, NULL);

K_WORK_DEFINE(zephyr_os_mgmt_reset_work, zephyr_os_mgmt_reset_work_handler);

#ifdef CONFIG_THREAD_MONITOR
static const struct k_thread *
zephyr_os_mgmt_task_at(int idx)
{
    const struct k_thread *thread;
    int i;

    thread = SYS_THREAD_MONITOR_HEAD;
    for (i = 0; i < idx; i++) {
        if (thread == NULL) {
            break;
        }
        thread = SYS_THREAD_MONITOR_NEXT(thread);
    }

    return thread;
}

int
os_mgmt_impl_task_info(int idx, struct os_mgmt_task_info *out_info)
{
    const struct k_thread *thread;
#if defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_STACK_INFO)
    size_t unused;
#endif

    thread = zephyr_os_mgmt_task_at(idx);
    if (thread == NULL) {
        return MGMT_ERR_ENOENT;
    }

    *out_info = (struct os_mgmt_task_info){ 0 };

#ifdef CONFIG_THREAD_NAME
    strncpy(out_info->oti_name, thread->name, OS_MGMT_TASK_NAME_LEN-1);
    out_info->oti_name[OS_MGMT_TASK_NAME_LEN - 1] = '\0';
#else
    ll_to_s(thread->base.prio, sizeof out_info->oti_name, out_info->oti_name);
#endif

    out_info->oti_prio = thread->base.prio;
    out_info->oti_taskid = idx;
    out_info->oti_state = thread->base.thread_state;
#ifdef CONFIG_THREAD_STACK_INFO
    out_info->oti_stksize = thread->stack_info.size / 4;
#ifdef CONFIG_INIT_STACKS
    if (k_thread_stack_space_get(thread, &unused) == 0) {
        out_info->oti_stkusage = (thread->stack_info.size - unused) / 4;
    } else {
        out_info->oti_stkusage = 0;
    }
#endif
#endif

    return 0;
}
#endif /* CONFIG_THREAD_MONITOR */

void __attribute__((weak))
app_received_reboot_command(unsigned int delay_ms)
{
    return;
}

void __attribute__((weak))
app_prepare_for_reboot(void)
{
    return;
}

static void
zephyr_os_mgmt_reset_work_handler(struct k_work *work)
{
    app_prepare_for_reboot();
    sys_reboot(SYS_REBOOT_WARM);
}

static void
zephyr_os_mgmt_reset_cb(struct k_timer *timer)
{
    /* Reboot the system from the system workqueue thread. */
    k_work_submit(&zephyr_os_mgmt_reset_work);
}

int
os_mgmt_impl_reset(unsigned int delay_ms)
{
    app_received_reboot_command(delay_ms);
    k_timer_start(&zephyr_os_mgmt_reset_timer, K_MSEC(delay_ms), K_NO_WAIT);
    return 0;
}
