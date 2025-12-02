MOUNT.AGENDAFS(8)

# NAME

mount.agendafs - Mount notes from vdir storages

# SYNOPSIS

*mount.agendafs* [-fdnrsvw] [*-o* _<OPTION>_] _MOUNTPOINT_

# DESCRIPTION

Agendafs is a virtual filesystem that loads vdir storages to a mountpoint, 
allowing you to create and edit notes.

# OPTIONS

Most mount options are described in _mount.fuse_(8) and _mount_(8). Below
are agendafs specific options:

*vdir=*<_path_>
	Use the specified vdir storage path as mountpoint. Currently
	only one calendar is supported. Required.

*ext=*<_format_>
	Automatically assign a file extension to files created outside
	of Agendafs. Disabled by default.

# XATTRIBUTES

Agendafs has support for xattributes (_xattr_(7)) to modify icalendar
fields. See the man pages of _setfattr_(1) and _getfattr_(1) for basic
usage. Built-in extended attributes are:

*user.categories*
	A comma separated list of categories to classify an entry.

*user.class*
	The classification of an entry. Can be either *public*,
	*private* or *confidential*. By default classification is set to
	*private*.
	
*user.sibling*
	Reserved for the future.

*user.uid*
	Immutable. The underlying UUID for the file. In the future, this
	can be used to reference notes with *user.sibling*.

*user.dtstart*
	Date-time start, ISO-formatted. Specifies the "beginning" of the
	file.

*user.status*
	Either *draft* or *final*. Can not be removed. By default, 
	directories are set to *final* while files are set to *draft*.

Arbitrary user-provided attributes are also supported. Limits are set to
255 bytes for attributes and 64 kilobytes for their value.

# FILE PERMISSIONS

Agendafs inherits the file permissions of the files in the underlying
vdir storage.

Currently hidden files (I.E. files prefixed with a dot) are not permitted
as they are reserved for future functionality.

# EXAMPLES

Mount vdir storage with agendafs to your home journal directory:

	```
	$ mount.agendafs \\
	  -o vdir=$HOME/.calendars/vjournal-calendar \\
	  $HOME/journal
	```

Mount vdir storage, and let files created outside of Agendafs automatically
use Markdown:

	```
	$ mount.agendafs \\
	  -o ext=md \\
	  -o vdir=$HOME/.calendars/vjournal-calendar \\
	  $HOME/journal
	```

Find all files with categories:

	```
	$ find . -type f -exec getfattr -n user.categories {} \\
	  + 2>/dev/null
	```

Find files by category MY_CATEGORY:

	```
	$ find . -type f -exec getfattr -n user.categories {} + 2>/dev/null \\
	  | grep -B1 "MY_CATEGORY" \\
	  | grep '^# file' \\
	  | awk '{ print substr($0, 9) }'
	```

The dtstart xattribute can be used for journaling. Simply set the xattribute
when creating a new file:

	```
	$ export FILE="Meeting with John"; export TIME=$(date +'%Y%m%d');
	$ touch "$FILE" && setfattr -n user.dtstart -v $TIME "$FILE"
	```

# SEE ALSO

_pimsync_(1)
_mount.fuse_(8)
_mount_(1)
_fusermount_(1)
_xattr_(7)
_getfattr_(1)
_setfattr_(1)
_agendafs-examples_(7)

vdirsyncer (https://vdirsyncer.pimutils.org/en/stabl)


# AUTHORS

Maintained by Marc Coquand <marc@coquand.email>. Up do date sources can be found at
https://git.sr.ht/~marcc/agendafs and bugs/patches can be submitted by email to
~marcc/agendafs@lists.sr.ht.

