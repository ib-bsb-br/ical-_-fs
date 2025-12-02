AGENDAFS-EXAMPLES(7)

# NAME

agendafs-examples - Ways to use agendafs

# Description

Agendafs can be combined with other tools to build your own *Personal
Knowledge Management* system. With so much power at your hands, it can
be hard to know what to do with it.

Below are some inspiration to get you started.

# Integrating with lf

_lf_(1) is a a terminal file manager available on most UNIX systems. It
works particularly well with agendafs.

## Showing categories in the promptfmt.

Categories are available from _getfattr_(1). The _lf_ prompt can be updated
to display the current categories of the selected file with three lines of
configuration added to lfrc.

	```
	cmd on-select &{{
		lf -remote "send $id set promptfmt \"%d $(getfattr -n user.categories --only-values "$f")\""
	}}
	```

## Showing class and status in the custom info

The following script uses _awk_(1) to parse a directory of files and updates
_lf_(1) to display a notification if a file is public and/or a draft

	```
	#!/usr/bin/sh
	cd "$(dirname "$1")"

	result=$(getfattr -d * | awk '
	    /^# file: / {
	        file = substr($0, 9);  
	        is_private = 0;
	        is_draft = 0;
	    }
	    
	    /^user.class="private"/ {
	        is_private = 1;
	    }
	 
	    /^user.status="draft"/ {
	        is_draft = 1;
	    }

	    END {
	        for (f in files) {
	            is_private = (files[f] ~ /user.class="private"/);
	            is_draft = (files[f] ~ /user.status="draft"/);

	            # Build the custom info string.
	            info = "";
	            if (is_private) {
	                info = info "p,";
	            }
	            if (is_draft) {
	                info = info "d,";
	            }
	            if (length(info) > 0) {
	                info = substr(info, 1, length(info) - 1);
	            }
	            if (length(info) > 0) {
	                printf "addcustominfo \"%s\" \"%s\"; ", f, info;
	            }
	        }
	    }
	    {
	        if ($0 ~ /^# file: /) {
	            file = substr($0, 9);
	            files[file] = "";
	        } else if (file != "") {
	            files[file] = files[file] " " $0;
	        }
	    }
	')

	if [ -n "$result" ]; then
	    lf -remote "send $id :$result"
	fi
	```

Add the script to $PATH along with the following to your lfrc:

	```
	set info custom
	cmd on-cd %{{
		lf_show_status "$1"
	}}
	cmd on-init %{{
		lf_show_status "$1"
	}}
	```

