/* 
** Distributed under the terms of the OpenBeOS License.
*/
#ifndef _SYSLOG_H_
#define _SYSLOG_H_


/**options for openlog() **/

#define LOG_PID			(1 << 12)	/* log the process (thread/team) ID with each message */
#define LOG_CONS		(2 << 12)	/* log to the system console on error */
#define LOG_ODELAY		(4 << 12)	/* delay open until syslog() is called */
#define LOG_NDELAY		(8 << 12)	/* connect to the syslog daemon immediately */
#define LOG_SERIAL		(16 << 12)	/* dump to serial output as well (not implemented) */
#define LOG_PERROR		(32 << 12)	/* dump to stderr as well */
#define LOG_NOWAIT		(64 << 12)	/* do not wait for child processes */


/** facilities **/

#define LOG_KERN		(0 << 3)	/* messages generated by the kernel */
#define LOG_USER		(1 << 3)	/* by user processes */
#define LOG_MAIL		(2 << 3)
#define LOG_DAEMON		(3 << 3)
#define LOG_AUTH		(4 << 3)
#define LOG_SYSLOG		(5 << 3)
#define LOG_LPR			(6 << 3)
#define LOG_NEWS		(7 << 3)
#define LOG_UUCP		(8 << 3)
#define LOG_CRON		(9 << 3)
#define LOG_AUTHPRIV	(10 << 3)

/* these are for local use: */
#define LOG_LOCAL0		(16 << 3)
#define LOG_LOCAL1		(17 << 3)
#define LOG_LOCAL2		(18 << 3)
#define LOG_LOCAL3		(19 << 3)
#define LOG_LOCAL4		(20 << 3)
#define LOG_LOCAL5		(21 << 3)
#define LOG_LOCAL6		(22 << 3)
#define LOG_LOCAL7		(23 << 3)


/** priorities **/

#define LOG_EMERG		0	/* a panic condition */
#define LOG_PANIC		LOG_EMERG
#define LOG_ALERT		1	/* a condition that should be corrected immediately */
#define LOG_CRIT		2	/* critical conditions like hard drive errors */
#define LOG_ERR			3
#define LOG_WARNING		4
#define LOG_NOTICE		5
#define LOG_INFO		6
#define LOG_DEBUG		7

/* turns a priority into a mask usable for setlogmask() */
#define LOG_MASK(pri)	(1 << (pri))


#ifdef __cplusplus
extern "C" {
#endif

// POSIX calls
extern void	closelog(void);
extern void	openlog(const char *ident, int options, int facility);
extern int	setlogmask(int priorityMask);
extern void	syslog(int priority, const char *message, ...);

// Be extensions
extern void	closelog_team(void);
extern void	openlog_team(const char *ident, int logopt, int facility);
extern void log_team(int priority, const char *message, ...);
extern int	setlogmask_team(int priorityMask);

extern void	closelog_thread(void);
extern void	openlog_thread(const char *ident, int logopt, int facility);
extern void log_thread(int priority, const char *message, ...);
extern int	setlogmask_thread(int priorityMask);

#ifdef __cplusplus
}
#endif

#endif	/* _SYSLOG_H_ */
