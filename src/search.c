/* Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * http://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 */

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include "config.h"
#include "screen.h"
#include "mark.h"
#include "mark.h"
#include "input.h"

#define INPUTLINE (flayer->l_height - 1)

bool search_ic;

/********************************************************************
 *  VI style Search
 */

static int matchword(char *, int, int, int);
static void searchend(char *, size_t, void *);
static void backsearchend(char *, size_t, void *);

void Search(int dir)
{
	struct markdata *markdata;
	if (dir == 0) {
		markdata = (struct markdata *)flayer->l_data;
		if (markdata->isdir > 0)
			searchend(0, 0, 0);
		else if (markdata->isdir < 0)
			backsearchend(0, 0, 0);
		else
			LMsg(0, "No previous pattern");
	} else
		Input((dir > 0 ? "/" : "?"), sizeof(markdata->isstr) - 1, INP_COOKED,
		      (dir > 0 ? searchend : backsearchend), NULL, 0);
}

static void searchend(char *buf, size_t len, void *data)
{
	int x = 0, sx, ex, y;
	struct markdata *markdata;
	Window *p;

	(void)data; /* unused */

	markdata = (struct markdata *)flayer->l_data;
	p = markdata->md_window;
	markdata->isdir = 1;
	if (len)
		strcpy(markdata->isstr, buf);
	sx = markdata->cx + 1;
	ex = flayer->l_width - 1;
	for (y = markdata->cy; y < p->w_histheight + flayer->l_height; y++, sx = 0) {
		if ((x = matchword(markdata->isstr, y, sx, ex)) >= 0)
			break;
	}
	if (y >= p->w_histheight + flayer->l_height) {
		LGotoPos(flayer, markdata->cx, W2D(markdata->cy));
		LMsg(0, "Pattern not found");
	} else
		revto(x, y);
}

static void backsearchend(char *buf, size_t len, void *data)
{
	int sx, ex, x = -1, y;
	struct markdata *markdata;

	(void)data; /* unused */

	markdata = (struct markdata *)flayer->l_data;
	markdata->isdir = -1;
	if (len)
		strcpy(markdata->isstr, buf);
	ex = markdata->cx - 1;
	for (y = markdata->cy; y >= 0; y--, ex = flayer->l_width - 1) {
		sx = 0;
		while ((sx = matchword(markdata->isstr, y, sx, ex)) >= 0)
			x = sx++;
		if (x >= 0)
			break;
	}
	if (y < 0) {
		LGotoPos(flayer, markdata->cx, W2D(markdata->cy));
		LMsg(0, "Pattern not found");
	} else
		revto(x, y);
}

static int matchword(char *pattern, int y, int sx, int ex)
{
	uint32_t *ip, *ipe, *cp;
	unsigned char *pp;
	struct mline *ml;

	/* *sigh* to make WIN work */
	fore = ((struct markdata *)flayer->l_data)->md_window;

	ml = WIN(y);
	ip = ml->image + sx;
	ipe = ml->image + flayer->l_width;
	for (; sx <= ex; sx++) {
		cp = ip++;
		pp = (unsigned char *)pattern;
		for (;;) {
			if ((char)*cp != *pp)
				if (!search_ic || ((*cp ^ *pp) & 0xdf) || (*cp | 0x20) < 'a' || (*cp | 0x20) > 'z')
					break;
			cp++;
			pp++;
			if (*pp == 0)
				return sx;
			if (cp == ipe)
				break;
		}
	}
	return -1;
}

/********************************************************************
 *  Emacs style ISearch
 */

static char *isprompts[] = {
	"I-search backward: ", "failing I-search backward: ",
	"I-search: ", "failing I-search: "
};

static int is_redo(struct markdata *);
static void is_process(char *, size_t, void *);
static int is_bm(char *, int, int, int, int);

static int is_bm(char *str, int l, int p, int end, int dir)
{
	int tab[256];
	int i, q;
	unsigned char *s, c;
	int w = flayer->l_width;

	/* *sigh* to make WIN work */
	fore = ((struct markdata *)flayer->l_next->l_data)->md_window;
	if (p < 0 || p + l > end)
		return -1;
	if (l == 0)
		return p;
	if (dir < 0)
		str += l - 1;
	for (i = 0; i < 256; i++)
		tab[i] = l * dir;
	for (i = 0; i < l - 1; i++, str += dir) {
		q = *(unsigned char *)str;
		tab[q] = (l - 1 - i) * dir;
		if (search_ic && (q | 0x20) >= 'a' && ((q | 0x20) <= 'z'))
			tab[q ^ 0x20] = (l - 1 - i) * dir;
	}
	if (dir > 0)
		p += l - 1;
	while (p >= 0 && p < end) {
		q = p;
		s = (unsigned char *)str;
		for (i = 0;;) {
			c = (WIN(q / w))->image[q % w];
			if (i == 0)
				p += tab[(int)(unsigned char)c];
			if (c != *s)
				if (!search_ic || ((c ^ *s) & 0xdf) || (c | 0x20) < 'a' || (c | 0x20) > 'z')
					break;
			q -= dir;
			s -= dir;
			if (++i == l)
				return q + (dir > 0 ? 1 : -l);
		}
	}
	return -1;
}

static void is_process(char *p, size_t len, void *data)
{				/* i-search */
	int pos, x, y, dir;
	struct markdata *markdata;

	(void)data; /* unused */

	if (len == 0)
		return;
	markdata = (struct markdata *)flayer->l_next->l_data;

	pos = markdata->cx + markdata->cy * flayer->l_width;
	LGotoPos(flayer, markdata->cx, W2D(markdata->cy));

	switch (*p) {
	case '\007':		/* CTRL-G */
		pos = markdata->isstartpos;
	 /*FALLTHROUGH*/ case '\033':	/* ESC */
		*p = 0;
		break;
	case '\013':		/* CTRL-K */
	case '\027':		/* CTRL-W */
		markdata->isistrl = 1;
	 /*FALLTHROUGH*/ case '\b':
	case '\177':
		if (markdata->isistrl == 0)
			return;
		markdata->isistrl--;
		pos = is_redo(markdata);
		*p = '\b';
		break;
	case '\023':		/* CTRL-S */
	case '\022':		/* CTRL-R */
		if (markdata->isistrl >= (int)sizeof(markdata->isistr))
			return;
		dir = (*p == '\023') ? 1 : -1;
		pos += dir;
		if (markdata->isdir == dir && markdata->isistrl == 0) {
			strcpy(markdata->isistr, markdata->isstr);
			markdata->isistrl = markdata->isstrl = strlen(markdata->isstr);
			break;
		}
		markdata->isdir = dir;
		markdata->isistr[markdata->isistrl++] = *p;
		break;
	default:
		if (*p < ' ' || markdata->isistrl >= (int)sizeof(markdata->isistr)
		    || markdata->isstrl >= (int)sizeof(markdata->isstr) - 1)
			return;
		markdata->isstr[markdata->isstrl++] = *p;
		markdata->isistr[markdata->isistrl++] = *p;
		markdata->isstr[markdata->isstrl] = 0;
	}
	if (*p && *p != '\b')
		pos =
		    is_bm(markdata->isstr, markdata->isstrl, pos,
			  flayer->l_width * (markdata->md_window->w_histheight + flayer->l_height), markdata->isdir);
	if (pos >= 0) {
		x = pos % flayer->l_width;
		y = pos / flayer->l_width;
		LAY_CALL_UP
		    (LayRedisplayLine(INPUTLINE, 0, flayer->l_width - 1, 0);
		     revto(x, y); if (W2D(markdata->cy) == INPUTLINE)
		     revto_line(markdata->cx, markdata->cy, INPUTLINE > 0 ? INPUTLINE - 1 : 1);) ;
	}
	if (*p)
		inp_setprompt(isprompts[markdata->isdir + (pos < 0) + 1], markdata->isstrl ? markdata->isstr : "");
	flayer->l_x = markdata->cx;
	flayer->l_y = W2D(markdata->cy);
	LGotoPos(flayer, flayer->l_x, flayer->l_y);
	if (!*p) {
		/* we are about to finish, keep cursor position */
		flayer->l_next->l_x = markdata->cx;
		flayer->l_next->l_y = W2D(markdata->cy);
	}
}

static int is_redo(struct markdata *markdata)
{
	int i, pos, npos, dir;
	char c;

	npos = pos = markdata->isstartpos;
	dir = markdata->isstartdir;
	markdata->isstrl = 0;
	for (i = 0; i < markdata->isistrl; i++) {
		c = markdata->isistr[i];
		if (c == '\022')	/* ^R */
			pos += (dir = -1);
		else if (c == '\023')	/* ^S */
			pos += (dir = 1);
		else
			markdata->isstr[markdata->isstrl++] = c;
		if (pos >= 0) {
			npos =
			    is_bm(markdata->isstr, markdata->isstrl, pos,
				  flayer->l_width * (markdata->md_window->w_histheight + flayer->l_height), dir);
			if (npos >= 0)
				pos = npos;
		}
	}
	markdata->isstr[markdata->isstrl] = 0;
	markdata->isdir = dir;
	return npos;
}

void ISearch(int dir)
{
	struct markdata *markdata;

	markdata = (struct markdata *)flayer->l_data;
	markdata->isdir = markdata->isstartdir = dir;
	markdata->isstartpos = markdata->cx + markdata->cy * flayer->l_width;
	markdata->isistrl = markdata->isstrl = 0;
	if (W2D(markdata->cy) == INPUTLINE)
		revto_line(markdata->cx, markdata->cy, INPUTLINE > 0 ? INPUTLINE - 1 : 1);
	Input(isprompts[dir + 1], sizeof(markdata->isstr) - 1, INP_RAW, is_process, NULL, 0);
	LGotoPos(flayer, markdata->cx, W2D(markdata->cy));
	flayer->l_x = markdata->cx;
	flayer->l_y = W2D(markdata->cy);
}
