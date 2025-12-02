#ifndef ical_extra_h_INCLUDED
#define ical_extra_h_INCLUDED
#include "libical/ical.h"
#include "arena.h"
#include "sys/stat.h"
#include "uuid/uuid.h"
#include <stdbool.h>

icaltimetype
get_last_modified(icalcomponent *component);

bool
is_directory_component(icalcomponent *component);

// Owner: ctx
icalcomponent *
parse_ics_file(arena *ar, const char *filename);

icaltimetype
get_ical_now();

size_t
icalcomponent_get_description_size(icalcomponent *component);

char *
create_new_unique_ics_uid(arena *ar);

icalcomponent *
create_vjournal_directory(arena *ar, const char *summary);

icalcomponent *
create_vjournal_entry(arena *ar, const char *summary);

const char *
get_parent_uid(icalcomponent *component);

void
remove_parent_child_relationship_from_component(icalcomponent *parent,
						icalcomponent *child);

void
set_parent_child_relationship_to_component(icalcomponent *parent,
					   icalcomponent *child);

const icalproperty_class
parse_ical_class(const char *input);

const icalproperty_status
parse_ical_status(const char *input);

const char *
format_ical_class(const enum icalproperty_class iclass);
const char *
format_ical_status(const enum icalproperty_status istatus);
void
icalcomponent_insert_description(arena *ar, icalcomponent *ic,
				 const char *buf, size_t size, off_t offset);

void
icalcomponent_set_file_extension(icalcomponent *component,
				 const char *extension);

const char *
icalcomponent_get_file_extension(icalcomponent *component);

void
icalcomponent_mark_as_directory(icalcomponent *ic);

void
icalcomponent_set_custom_x_value(arena *ar, icalcomponent *component,
				 const char *key, const char *value);

const char *
icalcomponent_get_custom_x_value(arena *ar, icalcomponent *component,
				 const char *key);

void
icalcomponent_remove_custom_prop(arena *ar, icalcomponent *component,
				 const char *key);

void
icalcomponent_print_x_props(FILE *memstream, icalcomponent *component);

#endif // ical_extra_h_INCLUDED
