/*
 *  (C) Copyright 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001,2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002,2003,2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004,2005,2006,2007 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

SNAC_SUBHANDLER(icq_snac_extension_error) {
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "extension", error);
	return 0;
}

#define METASNAC_SUBHANDLER(x) static int x(session_t *s, unsigned char *buf, int len, uint32_t uid, uint8_t retcode)
typedef int (*metasnac_subhandler_t) (session_t *s, unsigned char *, int, uint32_t, uint8_t ); 

#include "icq_fieldnames.c"

/* XXX, pododawac jakies ramki? */

METASNAC_SUBHANDLER(icq_snac_extensions_hpagecat) {
	debug_function("icq_snac_extensions_hpagecat() uid: %u\n", uid);
	return 0;
}

#define printq_userinfo(theme, param, paramval) print(theme, session_name(s), itoa(uid), param, paramval)

#define expand_display_byte(formatka, param)		\
	{									\
		uint8_t __value;						\
		if (!ICQ_UNPACK(&buf, "c", &__value)) return -1;		\
		if (__value) printq_userinfo(formatka, param, itoa(__value));	\
	}

#define expand_display_byteT(formatka, table, param)		\
	{									\
		uint8_t __value;						\
		if (!ICQ_UNPACK(&buf, "c", &__value)) return -1;		\
		if (__value) printq_userinfo(formatka, param, icq_lookuptable(table, __value));	\
	}

#define expand_display_word(formatka, param)		\
	{									\
		uint16_t __value;						\
		if (!ICQ_UNPACK(&buf, "w", &__value)) return -1;		\
		if (__value) printq_userinfo(formatka, param, itoa(__value));	\
	}

#define expand_display_wordT(formatka, table, param)		\
	{									\
		uint16_t __value;						\
		if (!ICQ_UNPACK(&buf, "w", &__value)) return -1;		\
		if (__value) printq_userinfo(formatka, param, icq_lookuptable(table, __value));	\
	}

#define expand_display_str(formatka, param) 		\
	{							\
		uint16_t __len;					\
		if (!ICQ_UNPACK(&buf, "w", &__len)) return -1;	\
		if (len < __len) return -1;			\
		if (__len > 0) {				\
			char *__str = xstrndup((char *) buf, __len); /* XXX, recode */ \
			if (__str[0]) printq_userinfo(formatka, param, __str);\
			buf += __len; len -= __len;		\
			xfree(__str);				\
		}						\
	}


METASNAC_SUBHANDLER(icq_snac_extensions_interests) {
	uint8_t count;
	int i;

	debug_function("icq_snac_extensions_interests() uid: %u\n", uid);

	if (retcode != 0x0A)
		return 0;

	if (!ICQ_UNPACK(&buf, "C", &count))
		return -1;

	/* 4 is the maximum allowed personal interests, if count is
	   higher it's likely a parsing error */

	if (count > 4)
		count = 4;

	for (i = 0; i < count; i++) {
		expand_display_wordT("icq_userinfo_interests", interestsField, "Interests");
		expand_display_str("icq_userinfo_interests", "InterestsStr");
	}
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_affilations) {
	uint8_t count;
	int i;

	debug_function("icq_snac_extensions_affilations() %u\n", uid);

	if (retcode != 0x0A)
		return 0;

	if (!ICQ_UNPACK(&buf, "C", &count))
		return -1;

	/* 3 is the maximum allowed backgrounds, if count is
	   higher it's likely a parsing error */

	if (count > 3)
		count = 3;

	for (i = 0; i < count; i++) {
		expand_display_wordT("icq_userinfo_affilations", pastField, "PastAff");
		expand_display_str("icq_userinfo_affilations", "PastAffStr");
	}

	if (!ICQ_UNPACK(&buf, "C", &count))
		return -1;

	/* 3 is the maximum allowed affiliations, if count is
	   higher it's likely a parsing error */

	if (count > 3)
		count = 3;

	for (i = 0; i < count; i++) {
		expand_display_wordT("icq_userinfo_affilations", pastField, "Aff");
		expand_display_str("icq_userinfo_affilations", "AffStr");
	}
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_basicinfo) {
	debug_function("icq_snac_extensions_basicinfo() %u\n", uid);
#define printq_userinfo_basic(param, paramval)	printq_userinfo("icq_userinfo_basic", param, paramval)

	if (retcode != 0x0A)
		return 0;

	expand_display_str("icq_userinfo_basic", "Nickname");
	expand_display_str("icq_userinfo_basic", "Firstname");
	expand_display_str("icq_userinfo_basic", "Lastname");
	expand_display_str("icq_userinfo_basic", "Email");
	expand_display_str("icq_userinfo_basic", "City");
	expand_display_str("icq_userinfo_basic", "State");
	expand_display_str("icq_userinfo_basic", "Phone");
	expand_display_str("icq_userinfo_basic", "Fax");
	expand_display_str("icq_userinfo_basic", "Street");
	expand_display_str("icq_userinfo_basic", "Cellular");
	expand_display_str("icq_userinfo_basic", "Zip");
	expand_display_word("icq_userinfo_basic", "Country");
	expand_display_word("icq_userinfo_basic", "Timezone");

	debug_error("icq_snac_extensions_basicinfo() more data follow: %u\n", len);
#warning "icq_snac_extensions_basicinfo()"
	/* XXX */
#undef printq_userinfo_basic
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_notes) {
	debug_function("icq_snac_extensions_notes() %u\n", uid);

	if (retcode != 0x0A)
		return 0;

	expand_display_str("icq_userinfo_notes", "About");
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_workinfo) {
	debug_function("icq_snac_extensions_workinfo() %u\n", uid);

	if (retcode != 0x0A)
		return 0;

	expand_display_str("icq_userinfo_work", "CompanyCity");
	expand_display_str("icq_userinfo_work", "CompanyStany");
	expand_display_str("icq_userinfo_work", "CompanyPhone");
	expand_display_str("icq_userinfo_work", "CompanyFax");
	expand_display_str("icq_userinfo_work", "CompanyStreet");
	expand_display_str("icq_userinfo_work", "CompanyZIP");
	expand_display_word("icq_userinfo_work", "CompanyCountry");
	expand_display_str("icq_userinfo_work", "Company");
	expand_display_str("icq_userinfo_work", "CompanyDepartment");
	expand_display_str("icq_userinfo_work", "CompanyPosition");
	expand_display_word("icq_userinfo_work", "CompanyOccupation");
	expand_display_str("icq_userinfo_work", "CompanyHomepage");
	expand_display_str("icq_userinfo_work", "CompanyZIP");
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_moreinfo) {
	debug_function("icq_snac_extensions_moreinfo() uid: %u\n", uid);
#define printq_userinfo_more(param, paramval)	printq_userinfo("icq_userinfo_more", param, paramval)

	if (retcode != 0x0A)
		return 0;

	expand_display_word("icq_userinfo_more", "Age");
	{
		uint8_t gender;
		if (!ICQ_UNPACK(&buf, "c", &gender)) return -1;
		if (gender) printq_userinfo_more("Gender", gender == 1 ? "Female" : "Male");
	}
	expand_display_str("icq_userinfo_more", "Homepage");
	{
		uint16_t year;
		uint8_t month, day;
		if (!ICQ_UNPACK(&buf, "w", &year)) return -1;
		if (year) debug_white("icq_snac_extensions_moreinfo() Year: %u\n", year);

		if (!ICQ_UNPACK(&buf, "c", &month)) return -1;
		if (month) debug_white("icq_snac_extensions_moreinfo() Month: %u\n", month);

		if (!ICQ_UNPACK(&buf, "c", &day)) return -1;
		if (day) debug_white("icq_snac_extensions_moreinfo() Day: %u\n", day);
	}
	expand_display_byteT("icq_userinfo_more", languageField, "Language1");
	expand_display_byteT("icq_userinfo_more", languageField, "Language2");
	expand_display_byteT("icq_userinfo_more", languageField, "Language3");

	debug_error("icq_snac_extensions_moreinfo() more data follow: %u\n", len);
#warning "icq_snac_extensions_moreinfo()"
	/* XXX */

#undef printq_userinfo_more
	return 0;
}

SNAC_SUBHANDLER(icq_snac_extension_replyreq_2010) {
	struct {
		uint16_t subtype;
		uint8_t result;
		unsigned char *data;
	} pkt;
	metasnac_subhandler_t handler;

	if (!ICQ_UNPACK(&pkt.data, "wc", &pkt.subtype, &pkt.result)) {
		debug_error("icq_snac_extension_replyreq_2010() broken\n");
		return -1;
	}

	switch (pkt.subtype) {
		case 0x00C8: handler = icq_snac_extensions_basicinfo; break;	/* Miranda: OK, META_BASIC_USERINFO */
		case 0x00F0: handler = icq_snac_extensions_interests; break;	/* Miranda: OK, META_INTERESTS_USERINFO */
		case 0x00E6: handler = icq_snac_extensions_notes; break;	/* Miranda: OK, META_NOTES_USERINFO */
		case 0x010E: handler = icq_snac_extensions_hpagecat; break;	/* Miranda: OK, META_HPAGECAT_USERINFO */
		case 0x00d2: handler = icq_snac_extensions_workinfo; break;	/* Miranda: OK, META_WORK_USERINFO */
		case 0x00DC: handler = icq_snac_extensions_moreinfo; break;	/* Miranda: OK, META_MORE_USERINFO */
		case 0x00FA: handler = icq_snac_extensions_affilations;	break;	/* Miranda: OK, META_AFFILATIONS_USERINFO */
		default:     handler = NULL;
	}

	if (!handler) {
		debug_error("icq_snac_extension_replyreq_2010() ignored: %.4x\n", pkt.subtype);
		icq_hexdump(DEBUG_ERROR, pkt.data, len);
		return 0;
	} else
		handler(s, pkt.data, len, -1, pkt.result);
		#warning "XXX, -1 uid!"

	return 0;
}

SNAC_SUBHANDLER(icq_snac_extension_replyreq) {
	struct {
		uint16_t len;
		uint32_t uid;
		uint16_t type;
		uint16_t id;
	} pkt;
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;

	unsigned char *tlv_data;
	int tlv_len;

	debug_function("icq_snac_extension_replyreq()\n");

	tlvs = icq_unpack_tlvs(buf, len, 0);

	if (!(t = icq_tlv_get(tlvs, 1)) || t->len < 10) {
		debug_error("icq_snac_extension_replyreq() broken(1)\n");
		icq_tlvs_destroy(&tlvs);
		return -1;
	}

	if (t->len + 4 != len) {
		debug_error("icq_snac_extension_replyreq() broken(1,5)\n");
		icq_tlvs_destroy(&tlvs);
		return -2;
	}

	tlv_data = t->buf;
	tlv_len = t->len;

	if (!icq_unpack(tlv_data, &tlv_data, &tlv_len, "wiwW", &pkt.len, &pkt.uid, &pkt.type, &pkt.id)) {
		debug_error("icq_snac_extension_replyreq() broken(2)\n");
		icq_tlvs_destroy(&tlvs);
		return -1;
	}

	debug("icq_snac_extension_replyreq() (rlen: %d) rlen2: %d uid: %d type: %d\n", len, pkt.len, pkt.uid, pkt.type);

	if (xstrcmp(s->uid+4, itoa(pkt.uid))) {
		debug_error("icq_snac_extension_replyreq() 1919 UIN mismatch: %s vs %ld.\n", s->uid+4, pkt.uid);
		icq_tlvs_destroy(&tlvs);
		return -2;
	}

	if (t->len - 2 != pkt.len) {
		debug("icq_snac_extension_replyreq() 1743 Size mismatch in packet lengths.\n");
		icq_tlvs_destroy(&tlvs);
		return -2;
	}

	switch (pkt.type) {
		case 2010:
			icq_snac_extension_replyreq_2010(s, tlv_data, tlv_len);		/* Miranda: STARTED */
			break;
		default:
			debug_error("icq_snac_extension_replyreq() METASNAC with unknown code: %x received.\n", pkt.type);
			break;
	}

	icq_tlvs_destroy(&tlvs);
	return 0;
}

SNAC_HANDLER(icq_snac_extension_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_extension_error; break;
		case 0x03: handler = icq_snac_extension_replyreq; break;	/* Miranda: OK */
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_extension_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
		return 0;
	} else
		handler(s, buf, len);

	return 0;
}

