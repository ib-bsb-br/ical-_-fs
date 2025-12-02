#include "ical_extra.h"
#include "libical/ical.h"
#include "arena.h"
#include "path.h"
#include "sys/stat.h"
#include "util.h"
#include "uuid/uuid.h"
#include <stdbool.h>
const char *IS_DIRECTORY_PROPERTY = "X-CALDAVFS-ISDIRECTORY";
const char *FILE_EXTENSION_PROPERTY = "X-CALDAVFS-FILEEXT";
const char *CUSTOM_PROPERTY_PREFIX = "X-CALDAVFS-CUSTOM-";
size_t CUSTOM_PROPERTY_PREFIX_LEN = 18;

const char *
icalcomponent_get_uniq_x_value(icalcomponent *component, const char *key)
{
	icalcomponent *inner =
	    icalcomponent_get_first_component(component, ICAL_ANY_COMPONENT);
	if (!inner) {
		return NULL;
	}

	for (icalproperty *prop =
		 icalcomponent_get_first_property(inner, ICAL_X_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property(inner, ICAL_X_PROPERTY)) {

		const char *prop_name = icalproperty_get_x_name(prop);
		if (prop_name && strcmp(prop_name, key) == 0) {
			LOG("FOUND PROP %s", key);
			return icalproperty_get_value_as_string(prop);
		}
	}
	return NULL;
}

icaltimetype
get_last_modified(icalcomponent *component)
{
	icalproperty *last_modified_prop = icalcomponent_get_next_property(
	    component, ICAL_LASTMODIFIED_PROPERTY);

	if (last_modified_prop) {
		icaltimetype ical_lastmodified =
		    icalproperty_get_lastmodified(last_modified_prop);
		return ical_lastmodified;
	}
	else {
		return icalcomponent_get_dtstamp(component);
	}
}

const icalproperty_class
parse_ical_class(const char *input)
{
	if (strcmp(input, "private") == 0) {
		return ICAL_CLASS_PRIVATE;
	}
	else if (strcmp(input, "public") == 0) {
		return ICAL_CLASS_PUBLIC;
	}
	else if (strcmp(input, "confidential") == 0) {
		return ICAL_CLASS_CONFIDENTIAL;
	}
	return ICAL_CLASS_NONE;
}

const icalproperty_status
parse_ical_status(const char *input)
{
        if (strcasecmp(input, "draft") == 0) {
                return ICAL_STATUS_DRAFT;
        }
        else if (strcasecmp(input, "final") == 0) {
                return ICAL_STATUS_FINAL;
        }
        else if (strcasecmp(input, "completed") == 0) {
                return ICAL_STATUS_COMPLETED;
        }
        else if (strcasecmp(input, "cancelled") == 0) {
                return ICAL_STATUS_CANCELLED;
        }
        else if (strcasecmp(input, "needs-action") == 0) {
                return ICAL_STATUS_NEEDSACTION;
        }
        else if (strcasecmp(input, "in-process") == 0) {
                return ICAL_STATUS_INPROCESS;
        }

        return ICAL_STATUS_NONE;
}

const char *
format_ical_class(const enum icalproperty_class iclass)
{
	if (iclass == ICAL_CLASS_PRIVATE) {
		return "private";
	}
	else if (iclass == ICAL_CLASS_PUBLIC) {
		return "public";
	}
	else if (iclass == ICAL_CLASS_CONFIDENTIAL) {
		return "confidential";
	}
	return NULL;
}

const char *
format_ical_status(const enum icalproperty_status istatus)
{
        if (istatus == ICAL_STATUS_DRAFT) {
                return "DRAFT";
        }
        else if (istatus == ICAL_STATUS_FINAL) {
                return "FINAL";
        }
        else if (istatus == ICAL_STATUS_COMPLETED) {
                return "COMPLETED";
        }
        else if (istatus == ICAL_STATUS_CANCELLED) {
                return "CANCELLED";
        }
        else if (istatus == ICAL_STATUS_NEEDSACTION) {
                return "NEEDS-ACTION";
        }
        else if (istatus == ICAL_STATUS_INPROCESS) {
                return "IN-PROCESS";
        }
        return NULL;
}

bool
is_directory_component(icalcomponent *component)
{
        icalcomponent *inner =
            icalcomponent_get_first_component(component, ICAL_ANY_COMPONENT);

        if (inner) {
                for (icalproperty *prop = icalcomponent_get_first_property(
                         inner, ICAL_RELATEDTO_PROPERTY);
                     prop != NULL; prop = icalcomponent_get_next_property(
                                       inner, ICAL_RELATEDTO_PROPERTY)) {

                        const char *reltype = icalproperty_get_parameter_as_string(
                            prop, "RELTYPE");
                        if (reltype && strcasecmp(reltype, "CHILD") == 0) {
                                return true;
                        }
                }
        }
        const char *is_directory =
            icalcomponent_get_uniq_x_value(component, IS_DIRECTORY_PROPERTY);
        return is_directory && strcmp(is_directory, "YES") == 0;
}

icalcomponent *
parse_ics_file(arena *ar, const char *filename)
{

	FILE *file = fopen(filename, "r");
	LOG("filename is %s", filename);

	struct stat st;
	// Each node corresponds to a file on the system.
	assert(stat(filename, &st) == 0);

	size_t size = st.st_size;
	char *buffer = rmalloc(ar, size + 1);

	size_t bytes_read = fread(buffer, 1, size, file);
	buffer[bytes_read] = '\0';
	fclose(file);

	icalcomponent *component = ricalcomponent_new_from_string(ar, buffer);

	return component;
}

icaltimetype
get_ical_now()
{
	return icaltime_from_timet_with_zone(time(NULL), 0,
					     icaltimezone_get_utc_timezone());
}

size_t
icalcomponent_get_description_size(icalcomponent *component)
{
	const char *description = icalcomponent_get_description(component);
	if (!description) {
		return 0;
	}
	else {
		size_t size = strlen(description);
		return size;
	}
}

char *
create_new_unique_ics_uid(arena *ar)
{
	uuid_t uuid;
	char uuid_str[37]; // UUIDs are 36 characters + null terminator
	uuid_generate(uuid);
	uuid_unparse(uuid, uuid_str);
	char *res = NULL;
	size_t wRes = rasprintf(ar, &res, "%s-caldavfs", uuid_str);
	assert(wRes != -1);

	return res;
}

icalcomponent *
create_vjournal_directory(arena *ar, const char *summary)
{

	icalcomponent *calendar = icalcomponent_new_vcalendar();

	icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
	icalcomponent_add_property(calendar,
				   icalproperty_new_prodid("-//caldavfs//EN"));

	icalcomponent *journal = icalcomponent_new_vjournal();
	char *id = create_new_unique_ics_uid(ar);

	icalcomponent_add_property(journal, icalproperty_new_uid(id));

	icalcomponent_add_property(
	    journal, icalproperty_new_dtstamp(icaltime_current_time_with_zone(
			 icaltimezone_get_utc_timezone())));
	icalcomponent_add_property(journal, icalproperty_new_summary(summary));

	icalproperty *private = icalproperty_new_class(ICAL_CLASS_PRIVATE);
	icalcomponent_add_property(journal, private);
	icalcomponent_set_status(journal, ICAL_STATUS_FINAL);

	icalproperty *is_directory = icalproperty_new_x(IS_DIRECTORY_PROPERTY);
	icalproperty_set_x_name(is_directory, IS_DIRECTORY_PROPERTY);
	icalproperty_set_x(is_directory, "YES");
	icalcomponent_add_property(journal, is_directory);

	icalcomponent_add_component(calendar, journal);
	return calendar;
}

icalcomponent *
create_vjournal_entry(arena *ar, const char *summary)
{

	icalcomponent *calendar = ricalcomponent_new_vcalendar(ar);

	icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
	icalcomponent_add_property(calendar,
				   icalproperty_new_prodid("-//caldavfs//EN"));

	// Step 2: Create a VJOURNAL entry
	icalcomponent *journal = icalcomponent_new_vjournal();
	char *id = create_new_unique_ics_uid(ar);

	icalcomponent_add_property(journal, icalproperty_new_uid(id));
	icalcomponent_add_property(journal,
				   icalproperty_new_class(ICAL_CLASS_PRIVATE));

	icalcomponent_add_property(
	    journal, icalproperty_new_dtstamp(icaltime_current_time_with_zone(
			 icaltimezone_get_utc_timezone())));
	char *file_extension = get_file_extension(ar, summary);
	if (!file_extension) {
		file_extension = "";
	}
	icalproperty *private = icalproperty_new_class(ICAL_CLASS_PRIVATE);
	icalcomponent_add_property(journal, private);

	icalcomponent_set_status(journal, ICAL_STATUS_DRAFT);

	char *without_extension = without_file_extension(ar, summary);

	icalcomponent_add_property(journal,
				   icalproperty_new_summary(without_extension));
	icalcomponent_set_file_extension(journal, file_extension);

	icalcomponent_add_property(journal, icalproperty_new_description(""));

	icalcomponent_add_component(calendar, journal);

	return calendar;
}

const char *
get_parent_uid(icalcomponent *component)
{
	icalcomponent *inner =
	    icalcomponent_get_first_component(component, ICAL_ANY_COMPONENT);

	for (icalproperty *prop = icalcomponent_get_first_property(
		 inner, ICAL_RELATEDTO_PROPERTY);
	     prop != NULL; prop = icalcomponent_get_next_property(
			       inner, ICAL_RELATEDTO_PROPERTY)) {
		const char *reltype =
		    icalproperty_get_parameter_as_string(prop, "RELTYPE");
		if (reltype && strcasecmp(reltype, "PARENT") == 0) {
			return icalproperty_get_value_as_string(prop);
		}
	}

	return NULL;
}

void
remove_parent_child_relationship_from_component(icalcomponent *parent,
						icalcomponent *child)
{

	const char *parent_uid = icalcomponent_get_uid(parent);

	icalcomponent *inner =
	    icalcomponent_get_first_component(child, ICAL_ANY_COMPONENT);

	icalproperty *prop =
	    icalcomponent_get_first_property(inner, ICAL_RELATEDTO_PROPERTY);
	while (prop != NULL) {
		icalproperty *next = icalcomponent_get_next_property(
		    inner, ICAL_RELATEDTO_PROPERTY);
		const char *value = icalproperty_get_relatedto(prop);
		icalparameter *reltype = icalproperty_get_first_parameter(
		    prop, ICAL_RELTYPE_PARAMETER);

		if (value && strcmp(value, parent_uid) == 0 && reltype &&
		    icalparameter_get_reltype(reltype) == ICAL_RELTYPE_PARENT) {
			icalcomponent_remove_property(inner, prop);
			icalproperty_free(prop);
		}

		prop = next;
	}
}

void
set_parent_child_relationship_to_component(icalcomponent *parent,
					   icalcomponent *child)
{
	icalproperty *child_related_to_parent =
	    icalproperty_new_relatedto(icalcomponent_get_uid(parent));
	icalparameter *reltype_child =
	    icalparameter_new_reltype(ICAL_RELTYPE_PARENT);
	icalproperty_add_parameter(child_related_to_parent, reltype_child);

	icalcomponent *inner =
	    icalcomponent_get_first_component(child, ICAL_ANY_COMPONENT);
	if (inner) {
		icalcomponent_add_property(inner, child_related_to_parent);
	}
}

void
icalcomponent_insert_description(arena *ar, icalcomponent *ic,
				 const char *buf, size_t size, off_t offset)
{

	const char *old_desc = icalcomponent_get_description(ic);
	if (!old_desc)
		old_desc = "";

	char *new_desc = rstrins(ar, old_desc, offset, buf, size);
	icalcomponent_set_description(ic, new_desc);
}

icalcomponent *
icalcomponent_get_innermost(icalcomponent *c)
{
	icalcomponent *inner =
	    icalcomponent_get_first_component(c, ICAL_ANY_COMPONENT);

	if (!inner) {
		inner = c;
	}
	return inner;
}

void
icalcomponent_remove_x_prop(icalcomponent *component, const char *key)
{
	icalcomponent *inner = icalcomponent_get_innermost(component);
	assert(inner);

	icalproperty *prop_iter =
	    icalcomponent_get_first_property(inner, ICAL_X_PROPERTY);
	while (prop_iter != NULL) {
		icalproperty *next_prop =
		    icalcomponent_get_next_property(inner, ICAL_X_PROPERTY);
		const char *prop_name = icalproperty_get_x_name(prop_iter);
		if (prop_name && strcmp(prop_name, key) == 0) {
			icalcomponent_remove_property(inner, prop_iter);
			icalproperty_free(prop_iter);
			break;
		}
		prop_iter = next_prop;
	}
}

void
icalcomponent_print_x_props(FILE *memstream, icalcomponent *component)
{
	icalcomponent *inner = icalcomponent_get_innermost(component);
	icalproperty *prop_iter =
	    icalcomponent_get_first_property(inner, ICAL_X_PROPERTY);
	while (prop_iter != NULL) {
		icalproperty *next_prop =
		    icalcomponent_get_next_property(inner, ICAL_X_PROPERTY);
		const char *prop_name = icalproperty_get_x_name(prop_iter);
		if (starts_with_str(prop_name, CUSTOM_PROPERTY_PREFIX)) {
			const char *key =
			    prop_name + CUSTOM_PROPERTY_PREFIX_LEN;
			fprintf(memstream, "user.%s", key);
			fputc('\0', memstream);
		}
		prop_iter = next_prop;
	}
}

void
icalcomponent_remove_custom_prop(arena *ar, icalcomponent *component,
				 const char *key)
{

	char *x_key = NULL;
	rasprintf(ar, &x_key, "%s%s", CUSTOM_PROPERTY_PREFIX, key);
	icalcomponent_remove_x_prop(component, x_key);
}

void
icalcomponent_set_unique_x_value(icalcomponent *component, const char *key,
				 const char *value)
{
	icalcomponent_remove_x_prop(component, key);
	icalcomponent *inner = icalcomponent_get_innermost(component);
	assert(inner);

	icalproperty *fileext_prop = icalproperty_new_x(key);

	icalproperty_set_x_name(fileext_prop, key);
	icalproperty_set_x(fileext_prop, value);

	icalcomponent_add_property(inner, fileext_prop);
}
void
icalcomponent_set_custom_x_value(arena *ar, icalcomponent *component,
				 const char *key, const char *value)
{
	char *x_key = NULL;
	rasprintf(ar, &x_key, "%s%s", CUSTOM_PROPERTY_PREFIX, key);

	icalcomponent_set_unique_x_value(component, x_key, value);
}

const char *
icalcomponent_get_custom_x_value(arena *ar, icalcomponent *component,
				 const char *key)
{
	char *x_key = NULL;
	rasprintf(ar, &x_key, "%s%s", CUSTOM_PROPERTY_PREFIX, key);

	return icalcomponent_get_uniq_x_value(component, x_key);
}

// Since empty values are considered null, we store a disallowed
// extension (..) to indicate that the system has explicitly not set an
// extension.
void
icalcomponent_set_file_extension(icalcomponent *component,
				 const char *extension)
{

	if (strcmp(extension, "") == 0) {
		icalcomponent_set_unique_x_value(component,
						 FILE_EXTENSION_PROPERTY, ".");
	}
	else {
		icalcomponent_set_unique_x_value(
		    component, FILE_EXTENSION_PROPERTY, extension);
	}
	LOG("Set X-CALDAVFS-FILEEXT to '%s' on component.", extension);
}

const char *
icalcomponent_get_file_extension(icalcomponent *component)
{
	const char *ext =
	    icalcomponent_get_uniq_x_value(component, FILE_EXTENSION_PROPERTY);

	if (ext && strcmp(ext, ".") == 0)
		return "";
	return ext;
}

void
icalcomponent_mark_as_directory(icalcomponent *ic)
{
	icalcomponent_set_unique_x_value(ic, IS_DIRECTORY_PROPERTY, "YES");
}
