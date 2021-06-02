#include <zephyr.h>
#include <string.h>
// #include <sys/printk.h>
#include <logging/log_ctrl.h>
#include <logging/log.h>
#include <app_memory/app_memdomain.h>
#include "ethernet_logging.h"

LOG_MODULE_REGISTER(ethernet_log_system);

static const uint8_t log_level_lut[] = {
	LOG_LEVEL_ERR, /* EXT_LOG_CRITICAL */
	LOG_LEVEL_ERR, /* EXT_LOG_ERROR */
	LOG_LEVEL_WRN, /* EXT_LOG_WARNING */
	LOG_LEVEL_INF, /* EXT_LOG_NOTICE */
	LOG_LEVEL_INF, /* EXT_LOG_INFO */
	LOG_LEVEL_DBG /* EXT_LOG_DEBUG */
};

static void ethernet_log_handler(enum ext_log_level level, const char *fmt, ...)
{
	struct log_msg_ids src_level = { .level = log_level_lut[level],
					 .domain_id = CONFIG_LOG_DOMAIN_ID,
					 .source_id = LOG_CURRENT_MODULE_ID() };

	va_list ap;

	va_start(ap, fmt);
	log_generic(src_level, fmt, ap, LOG_STRDUP_CHECK_EXEC);
	va_end(ap);
}

K_APP_BMEM(app_part) static ext_log_handler log_handler;

static uint32_t timestamp_get(void)
{
	return NRF_RTC1->COUNTER;
}

static uint32_t timestamp_freq(void)
{
	return 32768 / (NRF_RTC1->PRESCALER + 1);
}

static void log_ethernet_thread(void *p1, void *p2, void *p3)
{
	bool usermode = _is_user_context();

	k_sleep(K_MSEC(100));

	printk("\n\t---=< RUNNING ETHERNET LOGGER FROM %s THREAD >=---\n\n",
	       (usermode) ? "USER" : "KERNEL");


    log_handler = ethernet_log_handler;
	// ext_log_system_log_adapt();
	// ext_log_system_foo();

	wait_on_log_flushed();
}

static void log_ethernet_supervisor(void *p1, void *p2, void *p3)
{
	(void)log_set_timestamp_func(timestamp_get, timestamp_freq());

	log_ethernet_thread(p1, p2, p3);
}

K_THREAD_DEFINE(log_ethernet_thread_id, ETHERNET_LOGGING_THREAD_STACKSIZE,
		log_ethernet_supervisor, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 1);
