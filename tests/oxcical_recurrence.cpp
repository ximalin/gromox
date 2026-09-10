// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 grommunio GmbH
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <gromox/element_data.hpp>
#include <gromox/ical.hpp>
#include <gromox/oxcmail.hpp>
#include <gromox/util.hpp>
#include "../tools/staticnpmap.cpp"

#undef assert
#define assert(x) do { if (!(x)) { \
	fprintf(stderr, "assert failed: %s\n", #x); \
	return EXIT_FAILURE; \
} } while (false)

using namespace gromox;
using message_ptr = std::unique_ptr<MESSAGE_CONTENT, mc_delete>;

static size_t occurrences(const std::string &haystack, const char *needle)
{
	size_t count = 0;
	for (size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos;
	     pos += strlen(needle))
		++count;
	return count;
}

static message_ptr import_calendar(const char *input)
{
	auto text = strdup(input);
	if (text == nullptr)
		return {};
	ical calendar;
	auto loaded = calendar.load_from_str_move(text);
	free(text);
	if (!loaded)
		return {};

	oxcical_converter converter;
	converter.log_id = "oxcical-recurrence-test";
	converter.org_name = "test";
	converter.alloc = malloc;
	converter.get_propids = ee_get_propids;
	converter.username_to_entryid = oxcmail_username_to_entryid;
	return converter.ical_to_mapi_single(calendar);
}

static int exceptions_inherit_series_timezone()
{
	fprintf(stderr, "== exceptions_inherit_series_timezone\n");
	static const char input[] =
		"BEGIN:VCALENDAR\r\n"
		"VERSION:2.0\r\n"
		"PRODID:-//Gromox//oxcical test//EN\r\n"
		"BEGIN:VTIMEZONE\r\n"
		"TZID:Europe/Moscow\r\n"
		"BEGIN:STANDARD\r\n"
		"DTSTART:16010101T000000\r\n"
		"TZOFFSETFROM:+0300\r\n"
		"TZOFFSETTO:+0300\r\n"
		"END:STANDARD\r\n"
		"END:VTIMEZONE\r\n"
		"BEGIN:VEVENT\r\n"
		"UID:two-exception-series\r\n"
		"DTSTAMP:20260810T080000Z\r\n"
		"SEQUENCE:1\r\n"
		"SUMMARY:series\r\n"
		"DTSTART;TZID=Europe/Moscow:20260812T110000\r\n"
		"DTEND;TZID=Europe/Moscow:20260812T130000\r\n"
		"RRULE:FREQ=WEEKLY;BYDAY=WE;COUNT=8\r\n"
		"END:VEVENT\r\n"
		"BEGIN:VEVENT\r\n"
		"UID:two-exception-series\r\n"
		"DTSTAMP:20260810T080000Z\r\n"
		"SEQUENCE:1\r\n"
		"RECURRENCE-ID;TZID=Europe/Moscow:20260819T110000\r\n"
		"SUMMARY:moved earlier\r\n"
		"DTSTART;TZID=Europe/Moscow:20260819T090000\r\n"
		"DTEND;TZID=Europe/Moscow:20260819T110000\r\n"
		"END:VEVENT\r\n"
		"BEGIN:VEVENT\r\n"
		"UID:two-exception-series\r\n"
		"DTSTAMP:20260810T080000Z\r\n"
		"SEQUENCE:1\r\n"
		"RECURRENCE-ID;TZID=Europe/Moscow:20260902T110000\r\n"
		"SUMMARY:moved later\r\n"
		"DTSTART;TZID=Europe/Moscow:20260904T110000\r\n"
		"DTEND;TZID=Europe/Moscow:20260904T130000\r\n"
		"END:VEVENT\r\n"
		"END:VCALENDAR\r\n";
	auto message = import_calendar(input);
	assert(message != nullptr);

	oxcical_converter converter;
	converter.log_id = "oxcical-recurrence-test";
	converter.org_name = "test";
	converter.alloc = malloc;
	converter.get_propids = ee_get_propids;
	ical output;
	assert(converter.mapi_to_ical(*message, output));
	std::string text;
	assert(output.serialize(text) == ecSuccess);
	/*
	 * The exceptions must be stated in the same timezone as the series,
	 * otherwise their RECURRENCE-ID names an instant that no EXDATE of the
	 * series matches.
	 */
	assert(text.find("RECURRENCE-ID:") == std::string::npos);
	assert(occurrences(text, "RECURRENCE-ID;TZID=") == 2);
	/* Whatever Gromox writes, Gromox has to be able to read back. */
	assert(import_calendar(text.c_str()) != nullptr);
	return EXIT_SUCCESS;
}

int main()
{
	return exceptions_inherit_series_timezone();
}
