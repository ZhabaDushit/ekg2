/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *                          Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dynstuff.h"
#include "commands.h"
#include "msgqueue.h"
#include "sessions.h"
#include "stuff.h"
#include "xmalloc.h"

list_t msg_queue = NULL;

/*
 * msg_queue_add()
 *
 * dodaje wiadomo�� do kolejki wiadomo�ci.
 * 
 *  - session - sesja, z kt�rej wysy�ano
 *  - rcpts - lista odbiorc�w
 *  - message - tre�� wiadomo�ci
 *  - seq - numer sekwencyjny
 *
 * 0/-1
 */
int msg_queue_add(const char *session, const char *rcpts, const char *message, const char *seq)
{
	msg_queue_t *m = xmalloc(sizeof(msg_queue_t));

	m->session	= xstrdup(session);
	m->rcpts	= xstrdup(rcpts);
	m->message 	= xstrdup(message);
	m->seq 		= xstrdup(seq);
	m->time 	= time(NULL);

	return (list_add(&msg_queue, m) ? 0 : -1);
}

static LIST_FREE_ITEM(list_msg_queue_free, msg_queue_t *) {
	xfree(data->session);
	xfree(data->rcpts);
	xfree(data->message);
	xfree(data->seq);
	xfree(data);
}

/*
 * msg_queue_remove_uid()
 *
 * usuwa wiadomo�� z kolejki wiadomo�ci dla danego
 * u�ytkownika.
 *
 *  - uin.
 *
 * 0 je�li usuni�to, -1 je�li nie ma takiej wiadomo�ci.
 */
int msg_queue_remove_uid(const char *uid)
{
	list_t l;
	int res = -1;

	for (l = msg_queue; l; ) {
		msg_queue_t *m = l->data;

		l = l->next;

		if (!xstrcasecmp(m->rcpts, uid)) {
			LIST_REMOVE(&msg_queue, m, list_msg_queue_free);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_remove_seq()
 *
 * usuwa wiadomo�� z kolejki wiadomo�� o podanym numerze sekwencyjnym.
 *
 *  - seq
 *
 * 0/-1
 */
int msg_queue_remove_seq(const char *seq)
{
	int res = -1;
	list_t l;

	if (!seq) 
		return -1;

	for (l = msg_queue; l; ) {
		msg_queue_t *m = l->data;

		l = l->next;

		if (!xstrcasecmp(m->seq, seq)) {
			LIST_REMOVE(&msg_queue, m, list_msg_queue_free);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_free()
 *
 * zwalnia pami�� po kolejce wiadomo�ci.
 */
void msg_queue_free() {
	LIST_DESTROY(msg_queue, list_msg_queue_free);
	msg_queue = NULL;
}

/*
 * msg_queue_flush()
 *
 * wysy�a wiadomo�ci z kolejki.
 *
 * 0 je�li wys�ano, -1 je�li nast�pi� b��d przy wysy�aniu, -2 je�li
 * kolejka pusta.
 */
int msg_queue_flush(const char *session)
{
	list_t l;
	int sent = 0;

	if (!msg_queue)
		return -2;

	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;

		m->mark = 1;
	}

	for (l = msg_queue; l;) {
		msg_queue_t *m = l->data;
		session_t *s;

		l = l->next;

		/* czy wiadomo�� dodano w trakcie opr�niania kolejki? */
		if (!m->mark)
			continue;

		/* wiadomo�� wysy�ana z nieistniej�cej ju� sesji? usuwamy. */
		if (!(s = session_find(m->session))) {
			LIST_REMOVE(&msg_queue, m, list_msg_queue_free);
			continue;
		}

		if (session && xstrcmp(m->session, session)) 
			continue;

		command_exec_format(NULL, s, 1, ("/msg \"%s\" %s"), m->rcpts, m->message);

		LIST_REMOVE(&msg_queue, m, list_msg_queue_free);

		sent = 1;
	}

	return (sent) ? 0 : -1;
}

/*
 * msg_queue_count_session()
 *
 * zwraca liczb� wiadomo�ci w kolejce dla danej sesji.
 *
 * - uin.
 */
int msg_queue_count_session(const char *uid)
{
	list_t l;
	int count = 0;

	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;

		if (!xstrcasecmp(m->session, uid))
			count++;
	}

	return count;
}

/*
 * msg_queue_write()
 *
 * zapisuje niedostarczone wiadomo�ci na dysku.
 *
 * 0/-1
 */
int msg_queue_write()
{
	list_t l;
	int num = 0;

	if (!msg_queue)
		return -1;

	if (mkdir_recursive(prepare_pathf("queue"), 1))		/* create ~/.ekg2/[PROFILE/]queue/ */
		return -1;

	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;
		const char *fn;
		FILE *f;

		if (!(fn = prepare_pathf("queue/%ld.%d", (long) m->time, num++)))	/* prepare_pathf() ~/.ekg2/[PROFILE/]queue/TIME.UNIQID */
			continue;

		if (!(f = fopen(fn, "w")))
			continue;

		chmod(fn, 0600);
		fprintf(f, "v1\n%s\n%s\n%ld\n%s\n%s", m->session, m->rcpts, m->time, m->seq, m->message);
		fclose(f);
	}

	return 0;
}

/**
 * msg_queue_read()
 *
 * Read msgqueue of not sended messages.<br>
 * msgqueue is subdir ("queue") in ekg2 config directory.
 *
 * @todo	return count of readed messages?
 *
 * @todo	code which handle errors is awful and it need rewriting.
 *
 * @return	-1 if fail to open msgqueue directory<br>
 * 		 0 on success.
 */

int msg_queue_read() {
	struct dirent *d;
	DIR *dir;

	if (!(dir = opendir(prepare_pathf("queue"))))		/* opendir() ~/.ekg2/[PROFILE/]/queue */
		return -1;

	while ((d = readdir(dir))) {
		const char *fn;

		msg_queue_t m;
		struct stat st;
		string_t msg;
		char *buf;
		FILE *f;

		if (!(fn = prepare_pathf("queue/%s", d->d_name)))
			continue;

		if (stat(fn, &st) || !S_ISREG(st.st_mode))
			continue;

		if (!(f = fopen(fn, "r")))
			continue;

		memset(&m, 0, sizeof(m));

		buf = read_file(f, 0);

		if (!buf || xstrcmp(buf, "v1")) {
			fclose(f);
			continue;
		}

		if (!(m.session = read_file(f, 1))) {
			fclose(f);
			continue;
		}
	
		if (!(m.rcpts = read_file(f, 1))) {
			xfree(m.session);
			fclose(f);
			continue;
		}

		if (!(buf = read_file(f, 0))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}

		m.time = atoi(buf);

		if (!(m.seq = read_file(f, 1))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}
		
		msg = string_init(NULL);

		buf = read_file(f, 0);

		while (buf) {
			string_append(msg, buf);
			buf = read_file(f, 0);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.message = string_free(msg, 0);

		list_add(&msg_queue, xmemdup(&m, sizeof(m)));

		fclose(f);
		unlink(fn);
	}

	closedir(dir);

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
