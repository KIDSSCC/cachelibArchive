/**
 * TieMa(Tiny&Mini) DBM (like dbm-style database)
 * Copyright (C) 2010 heiyeluren. All rights reserved.
 *
 * tmdb is a easy use, high performance like dbm-style database
 *  
 * Use and distribution licensed under the BSD license.  See
 * the LICENSE file for full text.
 *
 * To learn more open-source code, visit: http://heiyeluren.googlecode.com
 * My blog: http://blog.csdn.net/heiyeshuwu
 *
 * $Id: tm_util.h 2010-4-11, 2010-5-7 23:22 , 2010-6-27 01:51 heiyeluren $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

/*
die():根据mess解释系统调用错误信息，
err_dump():通过die抛出系统调用错误信息
debug_log():打印某一类日志
*substr():拷贝一个字符字串
explode():将一个字符串按照某个分隔符进行划分
*strtolower():字母转小写
*strtoupper():字母转大写
strpos():确定一个字符的位置
strrpos():确定一个字符最后一次出现的位置
str_pad():将一个字符串用指定字符填充
str_repeat():拷贝一个字符串
trim():去除左右空格
ltrim():去除左空格
rtrim():去除右空格
is_numeric():判断是否是数字
getdate():获取当前日期
socket_set_nonblock():设置socket为非阻塞
lock_reg():对文件进行锁定操作
file_exists():检查某个文件是否存在
*/


/**
 * Common function define 
 *
 */

/* Die alert message */
void die(char *mess);
void err_dump(char *mess);
void debug_log(short level, char *msg);

/* substr - Sub string from pos to length */
char *substr( const char *s, int start_pos, int length, char *ret );

/* explode -  separate string by separator */
void explode(char *from, char delim, char ***to, int *item_num);

/* strtolower - string to lowner */
char *strtolower( char *s );

/* strtoupper - string to upper */
char *strtoupper( char *s );

/* strpos - find char at string position */
int strpos (const char *s, char c);

/* strrpos - find char at string last position */
int strrpos (const char *s, char c);

/* str_pad — Pad a string to a certain length with another string */
int str_pad(char *s, int len, char c, char *to);

/* str_repeat — Repeat a string */
int str_repeat(char input, int len, char *to);

/* trim - strip left&right space char */
char *trim( char *s );

/* ltrim - strip left space char */
char *ltrim( char *s );

/* ltrim - strip right space char */
char * rtrim(char *str);

/* is_numeric - Check string is number */
int is_numeric( const char *s );

/* Fetch current date tme */
void getdate(char *s);

/* Set socket nonblock */
int socket_set_nonblock( int sockfd );

/* File lock reister */
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);

/* check file exists */
int file_exists (const char *path);

/**
 * All lock operate define
 */
#define	read_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
#define	readw_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLKW, F_RDLCK, (offset), (whence), (len))
#define	write_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define	writew_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLKW, F_WRLCK, (offset), (whence), (len))
#define	un_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))

/**
 * Disk io system call map
 */
#define t_open(pathname, flags)				open((pathname), (flags))
#define t_close(fd)							close((fd))
#define t_read(fd, buf, count)				read((fd), (buf), (count))
#define t_write(fd, buf, count)				write((fd), (buf), (count))
#define t_sync(fd)							fsync((fd))
#define t_seek(fildes, offset, whence)		lseek((fildes), (offset), (whence))
#define t_lock(fd, operation)				flock((fd), (operation))


/**
 * Debug/Log level define
 */
#define LOG_LEVEL_DEBUG		1
#define LOG_LEVEL_TRACE		2
#define	LOG_LEVEL_NOTICE	3
#define LOG_LEVEL_WARNING	4
#define LOG_LEVEL_FATAL		5


/**
 * file lock
 */
#define tdb_rlock(fd)						flock((fd), LOCK_SH)
#define tdb_wlock(fd)						flock((fd), LOCK_EX)
#define tdb_unlock(fd)						flock((fd), LOCK_UN)



