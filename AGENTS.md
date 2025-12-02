Your tasks, as the AI ASSISTANT, are each and every necessary to implement the following 'blueprint/framework' to the given `agendafs` github fork:

`````blueprint
~~~
Integrating Agendafs with jtxBoard Features - Comprehensive Blueprint
Introduction and Overview
In this blueprint, we outline the integration of Agendafs - a FUSE-based calendar filesystem - with the features of the jtxBoard application, leveraging the latest iCalendar task draft enhancements. The goal is to port jtxBoard's journals, notes, and task management capabilities to a Linux environment (Debian 11 on ARM64, e.g. rk3588 boards) using Agendafs as the core engine, instead of the previous Kotlin-based logic layer. We will detail how Agendafs innovatively represents calendar data as files and directories, and how it can be extended or configured to support jtxBoard's advanced features: linked tasks (sub-tasks and dependencies), tagging, recurrence, progress tracking, participants, and status workflows. All relevant proposals from the IETF draft "ical-tasks-16" (which introduces new iCalendar components and properties for task management) are incorporated, including PARTICIPANT components, the VSTATUS update sub-component, the TASK-MODE property for automated status management, and new fields like ESTIMATED-DURATION, REASON, SUBSTATE, and expanded participation status codes[16][17].
We begin by examining Agendafs's architecture - how it uses extended file attributes (xattrs) to store metadata and how it organizes calendar items in a hierarchy that can express parent-child relationships. This forms the foundation for representing complex task relationships (like tasks with sub-tasks or prerequisites) directly in the filesystem structure. Next, we delve into the features from ical-tasks-16 and map them onto our design: for example, showing how a PARTICIPANT entry for a task could be represented as sub-objects in the filesystem (to handle multiple assignees or observers), or how a VSTATUS log of status changes might be stored. We then describe the process of replacing the prior Kotlin logic with Agendafs in our target environment, covering installation on Debian 11 (ARM64), and how Agendafs will interface with the rest of the system.
A significant portion of this blueprint is dedicated to Bash/TUI (Text User Interface) workflows that demonstrate how an end-user or system administrator can perform common jtxBoard operations using shell commands and text-based tools on the mounted Agendafs. These examples include creating and editing journal entries (VJOURNAL), adding new tasks (VTODO) and updating their status or due dates, linking tasks to create sub-tasks or dependencies, tagging items for categorization, and setting up recurring tasks - all through familiar commands (e.g. using echo, vim, ln, or setfattr). We will illustrate, step by step, how each action in the shell translates to underlying changes in the iCalendar data. For instance, marking a task as complete might involve setting an xattr which updates the STATUS:COMPLETED field and perhaps generates a new VSTATUS record for the completion event. By providing concrete CLI examples, we ensure that the blueprint is not only theoretical but directly actionable.
Integration with external tools, especially vdirsyncer, is also addressed. Agendafs is designed to work with the vdir (virtual directory) storage format for calendars[18], meaning it doesn't sync to CalDAV servers by itself but relies on a synchronization tool to push and pull changes. We will detail how vdirsyncer can be configured to synchronize the ICS files underlying Agendafs with a CalDAV server (e.g., Nextcloud or Fastmail), thereby keeping the filesystem view and all edits in sync with other devices (including the jtxBoard Android app if it's pointing to the same CalDAV source). Additionally, we consider shell-based tooling like scheduling scripts for notifications (replacing push notifications with desktop or email alerts), command-line calendar utilities for reminders, and even git for version controlling the calendar data if desired.
Finally, this blueprint examines security, atomicity, and data integrity implications. We analyze how using a FUSE filesystem and xattrs affects data safety - for example, ensuring that writes to files and xattrs are atomic to prevent corruption of ICS data, and that concurrent accesses (user edits vs. sync processes) are handled gracefully. We also ensure compliance with iCalendar standards: the solution will not violate the ICS format and will be forward-compatible with the new draft extensions (while gracefully degrading if a CalDAV server or client doesn't support them). Throughout the design, we discuss alternative approaches and the rationale for chosen solutions - weighing the pros and cons of different representations (like storing metadata in filenames vs. xattrs, or using plain files vs. subdirectories for complex components) and considering the user experience implications. This dialectical approach highlights the trade-offs and why the final design is optimal for our use case.
In summary, this comprehensive blueprint provides a step-by-step roadmap for porting jtxBoard's features to an Agendafs-based, terminal-centric workflow. It merges the best of both worlds: the interoperability and standardized data model of iCalendar (augmented by new task management capabilities), with the flexibility and scriptability of a Unix filesystem interface. By the end of this document, one should have a clear understanding of how to implement and use such a system, and why each design decision was made.
(In the sections that follow, we will use the term "ICS" to refer to iCalendar data files, and we will cite relevant standards and the draft specification to reinforce the design where appropriate.)
Agendafs Architecture and Innovations
Agendafs is a FUSE (Filesystem in Userspace) module that exposes calendar data (from CalDAV collections or local ICS files) as a mountable filesystem. The core idea is that calendar events, tasks, and journals become files or directories that you can manipulate with regular file operations. This section examines three key innovations in Agendafs's architecture:
* Extended Attributes (xattr) for Metadata: Agendafs uses file extended attributes to store and retrieve calendar properties (like summary, dates, status) as metadata on filesystem objects.
* Parent-Child Relationships via Hierarchical Structure: It represents relationships between calendar components (such as tasks and sub-tasks, or events and related tasks) using directory nesting or linking, making logical relations visible in the file tree.
* Intuitive Directory Layout: The way Agendafs organizes calendars, items, and component types into directories and files is designed for human navigation and logical grouping (e.g., by date or category), which is crucial for a good TUI experience.
Let's explore each of these in detail.
Extended Attributes as Calendar Metadata
Extended file attributes are a feature of modern filesystems that allow attaching name-value pairs to files or directories, beyond the standard attributes like size or modification time. In Linux, for example, xattrs are often used for storing metadata such as security labels (SELinux context), user-defined info, etc. According to the Linux manual, "extended attributes are name:value pairs associated permanently with files and directories, similar to environment variables for a process"[3]. They are not part of the file contents; instead, they are stored in the filesystem's metadata. Crucially, extended attributes can be read and written atomically and are persisted with the file.
Agendafs leverages xattrs to hold iCalendar properties of a calendar item. This means that rather than forcing the user to edit raw ICS text for every detail, many properties can be viewed or modified via simple xattr commands. For example, an event's SUMMARY might be accessible as an xattr on the event's file (e.g., user.summary), the start date as user.dtstart, the due date of a task as user.due, etc. This design cleanly separates the content of the note (which might correspond to the ICS DESCRIPTION or VJOURNAL body) from the metadata (title, dates, status, etc.), while keeping them in one place (the file). A user can list all available xattrs on a file using commands like getfattr -d <filename> (which would show all user-space xattr names and values for that file).
For instance, if we have a task file FixBug123.txt representing a VTODO item, running getfattr -n user.status -n user.due -n user.percent-complete FixBug123.txt might output:
# file: FixBug123.txt  
user.status="IN-PROCESS"  
user.due="20251215T170000Z"  
user.percent-complete="50"
This indicates the task's status, due date, and completion percentage are stored as attributes. Internally, Agendafs would ensure these map to the corresponding ICS properties (STATUS, DUE, PERCENT-COMPLETE) in the underlying .ics file for that task. If the user changes one of these (using setfattr), Agendafs catches that and updates the ICS data accordingly. Extended attributes are handled atomically by the kernel - writing an xattr replaces the previous value entirely[13] - so the update either succeeds fully or not at all, which is good for data integrity.
Why is this approach innovative and useful? One big advantage is that it lets the note content remain clean and focused. When you open a note file (say in vim or nano), you see only what you care to write (journal text, task description, etc.), rather than a clutter of ICS headers. Metadata like timestamps or categories are kept out-of-band in xattrs. Another advantage is scriptability: using shell commands, you can quickly query or modify these properties. For example, to find all tasks due before today, one could iterate over files and check the user.due attribute, rather than parse ICS in each file manually. We could use a command like:
find . -type f -name "*.todo" -exec bash -c '[[ "$(getfattr -n user.due --only-values "$1")" < "$CURRENT_DATE" ]] && echo "$1"' _ {} \;
(assuming $CURRENT_DATE is in YYYYMMDDT format and comparing lexicographically works for date-time strings in UTC format). This is a bit technical, but it shows that with xattrs, standard UNIX text processing tools can inspect task properties directly, as those properties are exposed by the filesystem.
Agendafs likely uses the "user." namespace for its extended attributes (as seen above, user.status, user.due, etc.), since the user namespace is available to regular processes for arbitrary data[19]. This means no special privileges are required to read or write those attributes, beyond owning the file. (In contrast, "security." or "system." xattr namespaces are restricted for admin or kernel use[20][21], which is not what we want for a personal notes filesystem.)
It's worth noting that extended attributes have size limits (values up to 64KB typically, and each filesystem may impose its own limit on total xattr bytes per file)[22][23]. In practice, ICS properties are small (text strings, dates, etc.), so this is not a concern. Agendafs wouldn't attempt to store the entire ICS text in an xattr - just the individual fields. The heavy content (like a long meeting minutes or journal entry text) remains the file body.
Example: Imagine creating a journal entry for today. With Agendafs mounted, you might do:
echo "Attended project kickoff meeting. Discussed requirements..." > Journal/2025-12-01.txt
setfattr -n user.dtstart -v "20251201" "Journal/2025-12-01.txt"
setfattr -n user.journal-type -v "meeting-minutes" "Journal/2025-12-01.txt"
The first command writes the content of the journal (which becomes the DESCRIPTION in ICS). The next command sets an xattr for the start date of the journal (ICS DTSTART property, date-only in this case), and the last sets a custom attribute to label the type of journal note (this could correspond to an ICS CATEGORIES or a custom PROPERTY, or simply a user-defined tag). After these, Agendafs will have created an ICS entry (a VJOURNAL component) under the hood with those details. A quick getfattr -d Journal/2025-12-01.txt might show:
user.dtstart="20251201"
user.journal-type="meeting-minutes"
No SUMMARY is shown because maybe for journal entries Agendafs uses the filename as the title (or it could auto-set SUMMARY to the date or first words of content). We will discuss naming shortly. The key takeaway here is that xattrs allow rich metadata to be set without manually editing the .ics structure.
It's important to preserve these extended attributes during normal file operations. Copying or moving files within the Agendafs mount should keep the xattrs intact (standard cp needs -a to preserve xattrs; within the same filesystem, mv retains them by default). If a user uses an editor that writes to a temporary file and renames (common for editors), Agendafs must handle the case where a new file is written (without xattrs) - likely it will treat that as content update and keep metadata if possible, or the editor might actually invoke FUSE setattr operations to modify the file in place. This is a nuance in implementation: ideally, Agendafs intercepts file renames and carries over the metadata, or encourages editors to use in-place editing (some editors have configurable write methods). This is an example of the low-level details we consider in the design: ensuring xattr metadata is not accidentally lost. However, since we control the environment (the user will know to use typical editors), and given that for journaling and tasks it's mostly content that changes frequently, not the metadata, this risk is manageable.
To sum up, Agendafs's xattr-based metadata storage provides a powerful and user-friendly way to manipulate calendar data. It effectively turns file attributes into an API for calendar properties, which we will exploit for implementing features like tagging, status changes, and linking (as those often boil down to adding or modifying ICS properties, which correspond to xattrs in our system).
Representing Parent-Child Relationships and Links
jtxBoard supports linking journals, notes, and tasks with each other - for example, you might have a project note that links to several task entries, or a task with sub-tasks (like a checklist or milestones). The iCalendar standard (with extensions from RFC 9253 and the new draft) supports these relationships using properties like RELATED-TO (with various relationship types such as PARENT, CHILD, or DEPENDS-ON) and a property called LINK for external or cross-component references[12][24]. The challenge is how to reflect those relationships in a filesystem model in an intuitive way.
Agendafs introduces the idea of using the filesystem hierarchy itself to denote certain relationships. There are a couple of approaches to this: - Use directories and subdirectories to imply parent-child relationships. - Use symbolic links or special files to represent a reference from one item to another (for relationships that aren't strict hierarchy).
Hierarchical directories for sub-tasks: One natural mapping is that if a task has defined sub-tasks (i.e., in ICS terms, a VTODO with children VTODOs related via RELATED-TO;RELTYPE=CHILD), we could represent the parent task as a directory, and each sub-task as a file within that directory. For example, suppose we have a task "Plan Event" which has two sub-tasks "Book Venue" and "Invite Participants". In Agendafs, under a calendar directory, we might see:
Tasks/  
└── Plan Event/  
    ├── Book Venue.todo  
    └── Invite Participants.todo
Here, "Plan Event" is a directory (perhaps with an .ics file or note representing the parent task's own details), and inside it are files for each child task (with .todo extension indicating they are tasks). This structure immediately signals the parent-child relationship. If the user creates a new file inside "Plan Event/" directory, Agendafs can intercept that creation and automatically set the ICS RELATED-TO property to link the new task to the parent's UID with RELTYPE=CHILD. Similarly, if the parent directory is renamed, Agendafs would ideally propagate that change to the parent task's SUMMARY (so the file structure and ICS data remain consistent). (We have to ensure renaming directories is handled carefully - likely Agendafs will allow it, updating the underlying SUMMARY or a "TITLE" xattr, since the directory name is effectively the task name for the user.)
Under the hood, what happens is: - "Plan Event" is a VTODO in ICS with its own UID. - "Book Venue" VTODO has a property RELATED-TO:<UID of Plan Event>;RELTYPE=CHILD. - Likewise "Invite Participants" has RELATED-TO:<UID of Plan Event>;RELTYPE=CHILD. The ICS standard suggests that using RELTYPE=CHILD implicitly means the referenced UID is the parent. Some implementations might also put a reciprocal reference (parent pointing to children), but it's not strictly required - the relationship is often one-way. However, we could also choose to add a RELATED-TO:<UID of Book Venue>;RELTYPE=CHILD in the parent as well (though logically that would actually mark the parent as a child of the subtask, which is not correct). Instead, if we wanted a backlink, we'd use RELTYPE=PARENT on the parent pointing to the subtask's UID. The upcoming draft or RFC 9253 might define PARENT as a valid RELTYPE. If it does, Agendafs could add RELATED-TO:<UID of Book Venue>;RELTYPE=PARENT in Plan Event's entry, but this redundancy isn't necessary for functionality. We just need a consistent way to navigate, which the filesystem already gives us (you find children under the parent directory).
The benefit of this approach is clear organization - when listing tasks, you can easily see which tasks are broken down into sub-tasks. It mirrors how jtxBoard itself might present sub-tasks nested under a parent task visually. It also aids in things like bulk operations: deleting a parent directory could prompt deletion of all sub-tasks (with confirmation), which matches an expectation that if you cancel a big task, maybe its subtasks are no longer relevant.
However, not all relationships are strict hierarchies. The ICS RELATED-TO also allows dependencies that are not exactly parent-child. For example, Task B might depend on Task A (cannot start until A is done), but B is not a subtask of A in a conceptual outline sense. That is represented by RELATED-TO:<UID A>;RELTYPE=DEPENDS-ON[12]. Representing such relationships in a single tree structure is tricky, because a dependency graph is not a simple hierarchy - it could even be cyclic or a web. For those, using directories might not make sense (you wouldn't put B inside A, because B is not a part of A, it just follows A). Instead, symbolic links or cross-references can be used.
Symlinks for cross-task dependencies: Agendafs could allow a special directory or attribute for links. One idea is to have a subdirectory within a task directory called "links" or "related" which contains symlinks to the tasks it's related to. For example:
Tasks/
├── TaskA.todo
├── TaskB.todo
└── TaskA/
    └── links/
        └── TaskB.todo -> ../TaskB.todo
This indicates Task B is related to Task A (perhaps "depends on" Task A). The symlink is an actual filesystem object that points to TaskB's file. When a user creates such a symlink (or uses an Agendafs command to link tasks), Agendafs can interpret that as an instruction to add a RELATED-TO property. Specifically, if we create a symlink under TaskA/links/ pointing to TaskB, Agendafs can add RELATED-TO:<UID of TaskB>;RELTYPE=DEPENDS-ON to TaskB's ICS (meaning TaskB depends on TaskA). Or possibly it adds it to TaskA's ICS with RELTYPE="..." depending on which direction we want. Actually, logically, if B depends on A, then in ICS B would have RELATED-TO:UID-A;RELTYPE=DEPENDS-ON (which reads as "B is related to A with dependency type" - i.e., B depends on A). So the symlink we created in A's folder implies the dependency in the other task. This might seem inverted (why place link under A then?), so another scheme could be to have a global "Links" directory or a command-line tool to establish relationships. But exposing it in the FS is nice for transparency. Perhaps a better scheme is:
Tasks/
├── TaskA.todo
└── TaskB.todo
TaskB.todo (extended attrs) -> user.depends="TaskA"
Meaning, just set an xattr on B indicating dependency. But xattr values would need to hold some reference (maybe the UID of A or a filename that Agendafs can resolve to a UID). This is doable: e.g., setfattr -n user.depends_on -v "<UID-of-A>" TaskB.todo. But expecting users to find UIDs is not friendly. That's where symlinks shine because they can naturally point to the actual file representing A.
Another approach is to not make a physical symlink, but allow hard links or multiple directory entries. However, hard linking a single ICS file into multiple places could get complicated with consistency (FUSE would need to decide if multiple directory entries for one ICS object are allowed). Symlinks are simpler as a read-only indicator of a relationship.
Agendafs might also implement a virtual directory that aggregates all links. For example, a directory /Links/ where any file you create is essentially a pointer establishing a relationship. But that might be over-engineering. Given the complexity, this blueprint will use a simpler assumption for linking: - Subtasks (hierarchical relationships) use nested directories as described for parent-child. - Dependencies or references can be managed with either symlinks in a "related" subfolder or by an explicit CLI tool that sets the appropriate xattr. We will provide a workflow example in the next sections for how a user could link tasks using the available methods.
Example: Suppose we want to indicate Task "Invite Participants" cannot start until Task "Book Venue" is done (a dependency). Using an xattr approach:
# Suppose we have environment variables with paths for convenience:
PARENT="Tasks/Plan Event/Book Venue.todo"
CHILD="Tasks/Plan Event/Invite Participants.todo"
setfattr -n user.depends_on -v "$(getfattr -n user.uid --only-values "$PARENT")" "$CHILD"
Here we assume each file has a user.uid attribute storing its ICS UID (which Agendafs would likely provide). We retrieve the UID of "Book Venue" and store it as the depends_on attribute of "Invite Participants". Agendafs would then translate that into RELATED-TO:<UID-of-BookVenue>;RELTYPE=DEPENDS-ON on the Invite Participants VTODO. If "Book Venue" is itself in the same parent directory, another design could be to interpret "presence in same directory" plus an attribute to mean sequence. However, that's stretching semantics; explicit is better.
How Agendafs enforces or reflects these relationships: One interesting aspect is whether Agendafs will enforce any behavior (like preventing opening "Invite Participants" until "Book Venue" is done - that would be overly intrusive, and not expected of a filesystem). Instead, it's simply tracking the info. Maybe it could show an attribute or different permission bits to indicate a task is "blocked" by another, but that might complicate matters. Likely it will just store the info and rely on clients (or the user's knowledge) to handle it.
Links between different component types: jtxBoard also links notes/journals with tasks. For example, you might have a journal entry (VJOURNAL) that relates to a task (VTODO) - perhaps meeting minutes that link to an action item task, or vice versa a task linking to a note with more context. ICS allows any component to RELATED-TO any other (the draft even mentions tasks related to events for time blocking[25]). In our FS, journals might be stored in a separate "Journals" directory (or mixed with tasks but likely separate by type). How to link across them? We could similarly allow symlinks or xattrs containing UIDs referencing across directories. For consistency, one might create a symlink of a task inside a journal's folder or vice versa. We must be careful not to accidentally duplicate the item. A symlink in FS is purely a pointer, so that's safe for representing "this is linked to that".
Given these complexities, a simpler but slightly manual method might be acceptable: using a naming convention or an ID reference in content. For example, writing in a journal note "Related Task: #1234" and have some script that knows #1234 refers to a task UID or title. But that's outside ICS semantics (that's more like user convention). Since we want to fully utilize ICS, we'll stick to ICS RELATED-TO with actual references.
In summary, Agendafs supports parent-child and other relationships by mimicking them in the filesystem layout: - Parent task as directory containing children tasks as files (for hierarchical relationships). - Potential use of symlinks or special attributes to represent non-hierarchical links (like dependencies or references), ensuring that adding such a link updates the ICS RELATED-TO property accordingly.
This innovation means a user browsing the filesystem can visually discern structure (e.g., they'll see a directory named after a big task, with smaller task files inside - an intuitive sign of sub-tasks). It also means certain actions (like moving a task file from one directory to another) could be interpreted by Agendafs as re-parenting a task to a new parent or project - something we'd document for power users. For consistency, some restrictions might apply: perhaps a task directory can only contain tasks (not arbitrary mix of events), and Agendafs might prevent deeply nested levels beyond what ICS can handle (though ICS doesn't strictly limit nesting, it's just tasks with tasks etc.).
Directory Structure and Organization of Calendars and Items
The third innovation is how Agendafs arranges the overall filesystem to make it logical. We consider how calendars map to top-level directories, how different component types (tasks, journals, events) are separated or combined, and how naming of files works.
Calendar Collections as Directories: Typically, CalDAV servers have separate collections for different item types or purposes (one for events, one for tasks, one for journals). For example, jtxBoard might use a specific CalDAV calendar for journals+notes (VJOURNAL) and another for tasks (VTODO), as hinted by DAVx⁵ documentation[26]. Agendafs, when pointed to a CalDAV via vdirsyncer, might mount a single calendar collection or multiple. We have options: - Mount each calendar collection as a separate directory under a common root. - Mount one calendar at a time (so the root of the FS is the content of that one calendar). - Combine multiple into one FS if desired.
A plausible structure is:
/mnt/agendafs/  (root of fuse mount)
├── PersonalJournal/   (one calendar collection for journals/notes)
│   ├── 2025-12-01.txt
│   ├── 2025-12-02.txt
│   └── ...
└── ProjectTasks/     (another calendar collection for tasks)
    ├── Plan Event/
    │   ├── Book Venue.todo
    │   └── Invite Participants.todo
    ├── Buy Groceries.todo
    └── ...
In this scenario, "PersonalJournal" and "ProjectTasks" are calendar names or IDs (possibly derived from the CalDAV calendar display name). Inside them, for the journal, we choose to organize by date (as the files are prefixed with dates). For tasks, we listed tasks and one nested directory for a task with subtasks.
Organizing by Date vs. Category: For journals, a common approach is to organize entries by date, since journals are often chronological. A flat list of dozens or hundreds of .txt files named by date could be unwieldy in one folder, so sometimes one might use year/month subdirectories (e.g., 2025/12/01.txt). Agendafs could do that virtually (present year dirs). But it might not - it might just list all journal entries as individual files, possibly named with date and an optional title fragment. The Lobsters comments indicated Agendafs is primarily for VJOURNALs and possibly considering a "single calendar.txt" view[27]. However, lacking that feature currently, we go with one-file-per-entry.
For tasks, other ways to group could be by status (maybe have subdirs "Completed" vs "Pending") or by categories (tag-based directories). These are not standard in ICS but could be done as virtual groupings. At least one needed grouping is tasks vs journals vs events, because they are different component types. It makes sense to not mix them in one directory. If the CalDAV is separate, then by default they won't mix. If they were in one, Agendafs might then have separate subdirs per component type.
Let's assume a scenario: the Nextcloud server has one calendar that allows VTODO and VJOURNAL. If Agendafs mounts that as one directory, we might need subfolders: e.g., JournalEntries/ and Tasks/ within it, so the user can easily see which are tasks (and maybe have .todo extension) vs journals (.txt or .md extension). Or Agendafs might incorporate the component type into the filename (like "NoteTitle.journal" vs "TaskTitle.todo"). Using extensions or naming conventions can help the user (and their editors) know what type of item they are dealing with. For example, naming journal files with .md (since they are markdown notes perhaps), tasks with .todo, events with .ics or .event. This also allows editors to pick proper syntax highlighting or for the user to filter with globs.
File Naming Conventions: A critical design decision is how to name files such that they are unique, descriptive, and stable. ICS items have a UID (a globally unique identifier string). We could use the UID as the filename, but that's typically a random UUID which is not user-friendly (e.g., a8f1234e-... .txt). Instead, using the SUMMARY (title) as filename is more user-friendly (as in the "Plan Event" and "Book Venue.todo" example above). However, two tasks could have the same summary. Also, renaming a file (which changes the summary if we want consistency) or editing the summary via xattr (should rename the file to reflect new title) are complexities to handle.
One possible approach is to incorporate both title and a short UID in the filename. For instance: FixBug123 [abcd1234].todo - where the part in brackets is part of UID (or an incremental number). That ensures uniqueness and traceability. But it clutters the name. Another approach is for Agendafs to quietly handle duplicates by appending a counter or so ("Task (1).todo"). It might not matter hugely because the user usually won't create two tasks with exactly the same name under the same parent context often, but it can happen (e.g., "Buy Milk" in a Shopping list repeated).
For this blueprint, we will assume Agendafs uses human-readable names primarily based on the SUMMARY or first line of text. The exact conflict resolution mechanism can be abstracted (could be as simple as appending a unique suffix). The user's question doesn't emphasize this, but focusing on it briefly in our design discussion shows thoroughness. The key is that the filesystem reflects the logical names of items, making it easy to navigate.
Directories for grouping by tags or categories: Suppose a user wants to view tasks by tag (category). Since ICS tasks can have a CATEGORIES property (comma-separated tags), one could create a virtual folder for each tag containing links to tasks with that tag. Agendafs currently might not implement this, but it's an interesting possible feature. For example:
/mnt/agendafs/ProjectTasks/
├── [Category]/
│   ├── Home/
│   │   ├── Buy Groceries.todo -> ../Buy Groceries.todo
│   │   └── Clean Garage.todo -> ../Clean Garage.todo
│   └── Work/
│       └── Plan Event.todo -> ../Plan Event/
└── (normal tasks and dirs as earlier)
Here, in a special "[Category]" directory, we have subfolders "Home" and "Work" (tag names), each containing symlinks to the actual tasks that have those tags. This gives an alternate view. The user could browse into "Work" and see all work-related tasks, though those tasks physically reside in the main list or by project. Maintaining this automatically would require Agendafs to update those symlinks whenever a task's category xattr changes. This might be beyond initial scope, but we mention it as an extension aligning with the request for tag manipulation workflows.
Even if not automatic, a user can manually create such tag directories and symlink tasks in. Because symlinks are just pointers, multiple categorization is possible (a task with two tags would appear in two tag directories via two symlinks). The actual ICS data would have both categories listed, unaffected by symlinks (the symlink is just a UI convenience, while the source of truth is the xattr or ICS property on the task). We will cover in a later section how a user could use shell tools to manage tags.
Summary of Directory Structure:
- Top-level: one directory per calendar collection (or per major category like "Tasks", "Journals", "Events" if all in one calendar). The exact layout might depend on how vdirsyncer stores things; usually, vdirsyncer's "filesystem storage" might store each item as an .ics file in one directory. If Agendafs is reading that, it could either mirror that flat directory or present a nicer grouping. We favor a user-friendly grouping, which might mean Agendafs is somewhat opinionated in presentation (which is fine - it's meant for interactive use). - Within a calendar directory: files and subdirs representing items. Possibly subdirectories for complex items (tasks with children, or perhaps grouping journals by year, etc.). Possibly special subdirs like [Category] or [Links] if implementing those views. - Filenames: reflect the item's title (Summary) or date or content snippet, with an appropriate extension indicating type.
This architecture means that when you mount Agendafs, you see something akin to a well-organized set of folders and files that you can navigate with cd and ls to understand your schedule and tasks.
To illustrate, imagine listing the root of the Agendafs mount:
$ ls /mnt/agendafs
PersonalJournal  ProjectTasks  WorkCalendar
If "WorkCalendar" is for events (VEVENTs), "ProjectTasks" for VTODOs, and "PersonalJournal" for VJOURNALs, you have separated contexts. Going into ProjectTasks and listing:
$ ls /mnt/agendafs/ProjectTasks
Buy Groceries.todo      Plan Event/      Website Redesign/
It shows one simple task file and two projects (Plan Event, Website Redesign as directories). You can then:
$ ls "/mnt/agendafs/ProjectTasks/Plan Event"
Book Venue.todo        Invite Participants.todo   Summary.txt
Perhaps inside a task directory, Agendafs could provide a special file like Summary.txt or Details.txt that represents the parent task's own content (if the directory itself can't be directly edited as a file). Alternatively, the directory itself might be a file that can be opened (some FUSE allow a directory to also have file content, but that's unusual; more likely, it would have a marker file). Let's assume it places a Summary.txt for the description of the parent task (Plan Event's notes). The two .todo files are sub-tasks.
This combination of directory and file representations might be a bit complex, but it's one way. Another way is to not make "Plan Event" a directory at all times; perhaps it becomes a directory only when sub-tasks are created. Possibly, Agendafs initially has "Plan Event.todo" as a file, and if a user tries to create a subtask, it converts "Plan Event.todo" into a directory transparently - maybe renaming the original file to something inside (like moving its content to Summary.txt). This is tricky but not impossible. Alternatively, they could always separate content and children: i.e., have Plan Event.todo as the file and allow subtask directories with a naming convention like Plan Event.todo.d/Book Venue.todo (this is clunky).
For conceptual clarity in this document, we treat parent tasks as directories (with possibly a file inside for their content). The exact implementation is beyond our scope, but the design principle holds: hierarchy in tasks is reflected as hierarchy in the filesystem.
So far, we have described how Agendafs uses xattrs for metadata, directories for relationships, and its general layout for calendars and entries. These architectural decisions underpin how advanced features will be realized. Next, we incorporate the specific new features from the ical-tasks-16 draft into this model, ensuring our design can accommodate them.
Incorporating iCalendar Task Extensions (Draft ical-tasks-16)
The IETF draft "Task Extensions to iCalendar" (ical-tasks-16) introduces a suite of enhancements to the core iCalendar (RFC 5545) for better task management[28][29]. These include new component types, properties, and parameter values which allow richer expression of task attributes like participants, status tracking, and scheduling rules. To ensure our Agendafs-based system is future-proof and feature-complete, we will integrate all relevant proposals from this draft. This section will outline each major extension and describe how it maps to or is represented in our design. Specifically, we cover:
* PARTICIPANT Components: A new component to represent people or resources associated with a task (beyond the organizer/attendee model)[30].
* VSTATUS Components: A new sub-component for recording status changes (a log of statuses, each with details like reason and substate)[2][31].
* TASK-MODE Property: A property on tasks to indicate automated server-side behavior for status updates (like auto-complete or auto-fail)[5][6].
* New Task Properties: Including ESTIMATED-DURATION (expected time to complete)[7], REASON (for status change rationale)[32], SUBSTATE (granular state detail)[10], and PERCENT-COMPLETE usage in participants.
* Enhanced Status Codes: Introduction of a "FAILED" status for tasks and corresponding participant status (PARTSTAT) values[33][34].
* Relationships and Identifiers: Use of REFID, LINK, and improved RELATED-TO relations like DEPENDS-ON and parent/child from RFC 9253[12] (though not new in this draft, they are complementary features we should support for linking tasks).
By weaving these into Agendafs, we ensure that users can utilize all these features via the filesystem interface, even if some features are not yet widely supported on all clients or servers. Our design will not break if the server doesn't understand them (the data will be preserved, as CalDAV servers typically ignore unknown properties rather than stripping them). Now, let's go through each extension:
Participant Components for Task Assignment and Observation
One limitation of classical VTODO in iCalendar was that it treated task "attendees" similar to event attendees, but tasks often have more complex roles - e.g., an assignee (the person responsible to do it), possibly multiple contributors, and also observers or stakeholders who aren't directly doing the task but need updates (like a manager watching progress). The draft addresses this by introducing a new component PARTICIPANT[30]. Instead of just an ATTENDEE property (which is a single line per person with an email and status), a PARTICIPANT is a multi-line component that can hold more info: you can include a name, contact info, role, status specific to that participant, and even location/resource info related to that participant.
Concretely, the draft's syntax addition is:
participantc = "BEGIN:PARTICIPANT" CRLF
               partprop *locationc *resourcec *statusc
               "END:PARTICIPANT" CRLF
with partprop extended to include PERCENT-COMPLETE and REASON (so each participant can have their own completion percentage and a reason code if they decline or fail)[1]. The VTODO (task) can contain multiple PARTICIPANT sub-components. This is a more structured way to handle multiple actors on a task than multiple ATTENDEE lines.
How do we integrate PARTICIPANT into Agendafs? There are two aspects: storing the data and representing it to the user. The participants of a task could be treated similarly to sub-objects of a task, since a PARTICIPANT component exists within a VTODO in the ICS hierarchy. We have a few choices: - Represent participants as files within a task's directory (similar to sub-tasks). For example, inside Plan Event/ directory, besides subtask files, we could have a Participants/ subdirectory containing files for each participant. - Represent participants as extended attributes on the task file (if we consider participants simpler, but they have multiple fields, so xattrs alone might not be enough unless we encode all their details in multiple attributes). - Represent participants in some consolidated text format (like a section within the task's description or a separate "Participants.md" file).
The cleanest might be a Participants subdirectory. E.g.:
ProjectTasks/Plan Event/Participants/
├── alice.participant
└── bob.participant
Each file could correspond to a PARTICIPANT component. The content of, say, alice.participant could be empty or could store a note specific to that participant (though participants usually wouldn't have a freeform note in ICS, mostly properties). Instead of content, we would rely on xattrs for participant details: - user.email = "alice@example.com" (some identifier, or perhaps use the filename as ID). - user.role = "REQ-PARTICIPANT" or NON-PARTICIPANT (the latter for observers)[35]. - user.partstat = "ACCEPTED" or "NEEDS-ACTION" etc. (the participation status) - with the extended value FAILED possible for VTODO now[33][34]. - user.percent-complete = "50" (if one participant has completed half their part). - user.reason = "<URI>" (if they have a reason for a decline or failure, as per new REASON property[36]). - Perhaps the participant's common name if needed (CN) could be derived from filename or an xattr.
This approach makes participants first-class objects one can inspect. For example, getfattr -d Plan\ Event/Participants/alice.participant might show Alice's status and percent complete.
Alternatively, participants could be simple enough to manage as lines, but ICS sees them as sub-components with their own possible sub-structure (like they themselves could have a VSTATUS inside them to log that participant's status changes, according to the draft[29][37]). That starts to become a deeper tree (Task -> Participant -> VSTATUS maybe). Representing that in FS is doable (like Plan Event/Participants/alice.participant/StatusUpdates/ directory). But before complicating, let's note usage scenarios:
* If a task has one assignee (Alice) and one observer (Manager Bob), the ICS would have two PARTICIPANT components. We want the user to be able to see that and perhaps update their statuses (like mark Bob as having acknowledged something or Alice as completed).
* The user might want to add a participant (assign a new person) or remove one (maybe reassign). They could do so by creating or deleting a .participant file.
Workflow example integration: To add a participant to a task:
touch "ProjectTasks/Plan Event/Participants/charlie.participant"
setfattr -n user.email -v "charlie@example.com" "ProjectTasks/Plan Event/Participants/charlie.participant"
setfattr -n user.role -v "REQ-PARTICIPANT" "ProjectTasks/Plan Event/Participants/charlie.participant"
setfattr -n user.partstat -v "NEEDS-ACTION" "ProjectTasks/Plan Event/Participants/charlie.participant"
This would instruct Agendafs to create a new PARTICIPANT entry for Charlie with those details (role required participant, and initially needs-action meaning not yet accepted/acknowledged).
If the user instead did something like:
echo "charlie@example.com,ROLE=REQ-PARTICIPANT" > "ProjectTasks/Plan Event/Participants/charlie.participant"
maybe Agendafs could parse that, but relying on structured content is brittle; xattrs are clearer for structure. We will stick to the xattr approach for participants.
Link to actual scheduling & CalDAV: It's worth noting that participants correspond to iCalendar scheduling (iTIP) when you assign tasks. If this system is used with a CalDAV server that supports tasks, adding participants would conceptually mean you might want to invite those participants - but tasks assignment via CalDAV isn't as universally adopted as meeting invitations for events. The draft and CalDAV specs do discuss task assignment. The TASK-MODE property (discussed soon) deals with how server treats these for automatic status changes upon participant replies[38][39]. In our offline-first usage, adding a participant will mainly serve as documentation unless the server or some client triggers an email invitation. We won't delve into sending notifications; we focus on storing the info and letting the server handle it if it does (CalDAV might send email if configured, or we manually notify).
Participants as Observers (NON-PARTICIPANT role): The draft identifies an "observer" concept (someone interested in the task's progress but not performing it)[35]. In ICS, that is typically an ATTENDEE with ROLE=NON-PARTICIPANT. With PARTICIPANT components, we'd likely set user.role = "NON-PARTICIPANT" for such entries. They can have partstat too (usually observers might stay NEEDS-ACTION or something or have their own acceptance of being an observer). All this is supported by ICS.
Conclusion on Participant mapping: We will incorporate participant support by treating them as child objects of a task. A user can manage them by navigating into a Participants subdirectory and using familiar file operations. This aligns with our general design: subordinate ICS components (like VALARM, VSTATUS, PARTICIPANT) become subordinate filesystem elements. The presence of multiple layers (task -> participant -> maybe participant's statuses) hints at how flexible this can be. For now, we note that, and we'll handle VSTATUS next which also can occur under tasks or under participants.
VSTATUS Components for Detailed Status Tracking
The VSTATUS component introduced by the draft is essentially a way to record a status update as its own mini-component, which can be attached to a parent component (like a task, event, or participant)[40][1]. Each VSTATUS can include a STATUS (like COMPLETED, CANCELLED, etc.), a timestamp (implicitly its inclusion order, or perhaps an explicit date property if any), a REASON (URI explaining why status changed), and a SUBSTATE for finer detail (like ERROR or SUSPENDED)[10][11]. The concept is similar to having a history or log of status changes, including outcomes.
Normally, a VTODO has one primary STATUS property (which can be updated). Without VSTATUS, if a task's status changes, you lose the old status. VSTATUS allows preserving the sequence of changes - e.g., Task was IN-PROCESS at one point but later marked FAILED with reason X. This is very useful for auditing and for multi-participant tasks to see each person's completion events.
Representing VSTATUS in FS: We can treat each VSTATUS as a sub-item under the thing it relates to. The draft allows VSTATUS to appear within VEVENT, VTODO, VJOURNAL, VFREEBUSY, and PARTICIPANT components[40][41] - basically anywhere to log status. For our focus, the main usage is likely within VTODO (task) and possibly within PARTICIPANT (to log an individual's status change). For example, if a task fails, you might attach a VSTATUS to it with STATUS:FAILED, REASON:<some URL> explaining failure cause, SUBSTATE:ERROR if applicable[31].
In Agendafs, if a task is a directory (like Plan Event), we could have a subdirectory or file group for status history. Perhaps:
ProjectTasks/Plan Event/StatusHistory/
└── 2025-12-10T09:00.status
Where the file name could include a timestamp or sequence number, and .status extension indicates it's a VSTATUS record. Inside that file (or via xattrs on it), we'd store the details: - Content could be empty or a short human description. - xattrs: user.status = "FAILED", user.reason = "https://example.com/reason/no-venue", user.substate = "ERROR". Possibly also a user.date = "<timestamp>" if we want to store when this status was recorded (though ICS might not have an explicit timestamp property for VSTATUS beyond the sequence in file; we could use the DTSTAMP of the parent or just rely on file's mod time). The example from the draft shows the VSTATUS usage:
BEGIN:VSTATUS
STATUS:FAILED
REASON:https://example.com/reason/no-one-home
SUBSTATE:ERROR
END:VSTATUS
[31]. If another status happened:
BEGIN:VSTATUS
STATUS:IN-PROCESS
REASON:https://example.com/reason/paint-drying
SUBSTATE:SUSPENDED
END:VSTATUS
[4], that would be another record (perhaps earlier or later).
Alternatively, we could have simply numbered files "1.status", "2.status" in order. But having a timestamp in name is informative for the user. We could base it on the effective date of the status change (which might be now). Using ISO datetime in filename ensures lexicographical sort = chronological sort. For example, 2025-12-01T10:00.status could denote a status set at that time.
A user likely wouldn't manually create these in most cases; instead, when they mark a task completed or failed via our CLI, Agendafs could auto-generate a VSTATUS entry. However, if a user wanted to manually add a log entry (maybe retroactively logging something), they could create a file in the StatusHistory folder and set the attributes.
Status vs. VSTATUS usage: We should clarify: the main STATUS property of the task typically reflects the current status (e.g., after all changes, what's the final status). The VSTATUS components provide a history or additional information. For instance, if a task's DUE passed and it wasn't done, server might mark it as FAILED automatically (with an Automatic-Failure mode, to be discussed) and include a VSTATUS for that failure. The task might later be reopened, etc., generating multiple VSTATUS logs. Our design should allow multiple VSTATUS per task.
Therefore, having a StatusHistory directory or similar is appropriate. Alternatively, we could just list them in the task directory maybe with a certain naming. But having a separate directory keeps it organized and avoids clutter among participant files or subtask files.
If participants also have VSTATUS (imagine each participant might log when they complete their portion or fail), we might mirror the structure:
ProjectTasks/Plan Event/Participants/alice.participant/StatusHistory/...
So a participant file could also have a subdirectory for that participant's status changes. This is indeed indicated by the draft: "This component may be added to the PARTICIPANT component to allow participants in a task to specify their own status."[42]. So yes, a participant can contain VSTATUS as well.
This nested structure is deep but manageable: It's not more than 3-4 levels: ProjectTasks (calendar) -> Plan Event (task dir) -> Participants -> Alice (participant file or dir) -> StatusHistory -> 1.status file.
One could argue we could flatten participant status into the main StatusHistory, but that might mix up statuses from different people. So better keep them separate at the participant level.
Implication for UI: If a user is interested in the status history of a task, they can ls Plan\ Event/StatusHistory/ and see entries, or just check getfattr -d Plan\ Event/ if perhaps we also aggregate something (though likely not aggregated). For participant status, they'd go into participant's own structure.
Alternate approach (not chosen): Represent VSTATUS as version control of the STATUS xattr (like keep previous values). That would be hidden, not visible, so not as user-friendly. The explicit file representation is more transparent and lets a user even manually add explanatory notes in the content if desired (like writing a brief note in the .status file about context of the status change).
Integrating VSTATUS into sync: These VSTATUS components will be part of the ICS file. A CalDAV server not aware of them might store them anyway (since unknown sub-components typically are saved and delivered back, as long as they're in the same VCALENDAR). If a client (like an old one) sees them, it might ignore them. There's minimal harm in including them. We should ensure vdirsyncer is set to not strip unknown components (vdirsyncer usually doesn't alter content, it just syncs files as blobs unless told to filter). So we are fine.
TASK-MODE Property and Automated Task Management
The TASK-MODE property is a significant addition for task workflow. It tells the server how to auto-process the task's status based on participants' actions or time. The draft defines these values for TASK-MODE[43]: - CLIENT: (default if absent) - Manual mode, clients (organizer) manage status. No automation. - AUTOMATIC-COMPLETION: Server should automatically set the task's overall status to COMPLETED when all participants have completed (i.e., when every participant's PARTSTAT = COMPLETED)[6]. - AUTOMATIC-FAILURE: Server should automatically set status to FAILED if any participant fails or if the task's due date passes without completion[44][45]. - AUTOMATIC: A shorthand meaning do both of the above (auto-complete and auto-fail)[46]. - SERVER: Possibly similar to AUTOMATIC (the draft lists it in the ABNF but does not explicitly explain it in the snippet we saw; it might be a legacy or general "server decides" value). - Additionally, the draft allows for other IANA-registered modes or custom ones.
TASK-MODE is set on the VTODO. If, for example, we set TASK-MODE:AUTOMATIC-COMPLETION on a task with two participants, the CalDAV server (if it supports this spec) would monitor the participant statuses. The moment all participants mark themselves completed (or the organizer marks them completed on their behalf), the server would flip the task's STATUS to COMPLETED automatically[47][6]. Similarly, Automatic-Failure could mark it failed if overdue or someone fails. This is like server-side business logic for tasks, reducing manual oversight.
Representation in our system: TASK-MODE is just a property of the task. We can expose it as an extended attribute on the task file or directory. For example, user.task_mode = "AUTOMATIC-COMPLETION". A user could set that easily:
setfattr -n user.task_mode -v "AUTOMATIC-COMPLETION" "ProjectTasks/Plan Event"
(or if the task is a file, that file's xattr). We should ensure the possible values are constrained; likely, we will document the valid tokens. If a user tries something else, the server might reject it on upload, but locally we can allow it (since unknown iana-token could be for future or custom modes).
When a user creates a new task, we might default TASK-MODE to "CLIENT" (explicitly or just leave it absent which implies client). If they want to delegate it fully to server, they choose one of the auto modes. Note that these modes require the server to support them (the draft says clients must be prepared for server to reject if not supported[14][15], typically with a 403 and an error element indicating supported modes[48][15]). vdirsyncer will just sync it; the CalDAV server might respond with an error if it doesn't accept the property on PUT. This is something to consider: if using an older server, pushing a VTODO with TASK-MODE might fail. The client (our system) should handle that - perhaps by noticing that the file didn't sync, or vdirsyncer logs an error. This is outside Agendafs (which doesn't talk to server directly), but as a user, they might see a sync error. They could either remove the property or upgrade server. For our blueprint, we assume a server that either supports these or at least stores them.
No direct UI needed beyond setting the xattr, because the effect is server-side. We might want to reflect current mode in listing. Possibly in a directory listing, we could incorporate something (like the file could have a suffix "[Auto]" in the name if automatic? That's too tricky and not needed; easier is just check the xattr if interested).
One interesting interaction: If TASK-MODE is automatic, the server will change the STATUS. That means a sync from server might bring a new STATUS for a task. Good - our system will reflect that by updating the user.status xattr. But if a user is editing offline and marks done, need to be careful not to conflict. Generally, we rely on server to do its job and keep the final status consistent.
We should mention an example scenario in explanation: For instance, Scenario: Task assigned to two people, TASK-MODE=AUTOMATIC-COMPLETION. Initially, Status = NEEDS-ACTION. Participant Alice finishes and marks COMPLETE (so her participant partstat becomes COMPLETED). Participant Bob finishes later and marks COMPLETE. At that moment, server automatically sets overall task STATUS to COMPLETED (maybe adding a VSTATUS entry like in the example table in the draft that shows multi-attendee status progression[49][50]). Our system, upon next sync, would receive this update - the task's user.status becomes COMPLETED without the organizer manually doing it. The VSTATUS log might also come through showing step 7 "Overall state set by server for automatic completion"[51][50]. All this will be preserved.
To incorporate in design: We ensure that user.status xattr on tasks can be changed externally (and we refresh it after sync). Also, to avoid confusion, if a user tries to manually set status on an automatic task, the server might override or that might break the logic. The draft says if a client sets a status when server is expected to, unclear but presumably server would accept it if it's consistent or could turn off automation. However, the spec might expect that if you set a status manually, you might as well not use automatic mode. We might caution the user that if TASK-MODE is automatic, they should normally let statuses be handled by participant updates or time, rather than manually flipping the main status.
UI: Possibly we highlight tasks that are waiting for server automation. But that's an enhancement beyond simply storing the info. For now, just giving access to set the property is enough.
New Properties: Estimated-Duration, Reason, Substate, etc.
The draft introduces a few new properties to enhance task specification: - ESTIMATED-DURATION: A duration of time indicating how long the task is expected to take (separately from deadlines)[7][8]. For instance, "This task should take about 2 hours of work." It's a DURATION value (like "PT2H") and can be used by scheduling tools to visualize workload[52][53]. - REASON: A URI (could be a link to more information) explaining why a certain status change occurred or why a participant's status is what it is[36]. For example, if a participant declines a task, they might include a REASON link to a document or code enumerating reasons ("out of office", etc.). In VSTATUS, REASON might point to a more detailed incident report for failure. - SUBSTATE: A text field to qualify the status with a finer grain state[54]. This is useful especially when a task is IN-PROCESS - e.g., substate "SUSPENDED" could mean work is paused waiting on something[11], or substate "ERROR" means progress halted due to an error condition. The draft defines allowed values "OK", "ERROR", "SUSPENDED", etc.[11]. - Extended PERCENT-COMPLETE usage: Now allowed on PARTICIPANT (to track each participant's progress)[55]. VTODO already had a PERCENT-COMPLETE property at the task level. So conceptually, participant A might be 50% done with their part while overall task percent might also be tracked or computed.
In our system, each of these maps to an extended attribute on the relevant object: - user.estimated_duration: on a task file/dir (value e.g. "PT2H" or "P3D" etc.). Users can set it to indicate the expected effort. A shell user might not often set this unless they plan with it, but it's there for completeness. If we had a UI to sum up tasks, we could accumulate these, but that's beyond scope. - user.reason: on a VSTATUS file or a participant file, when relevant. For instance, if a participant's partstat is DECLINED, we could set user.reason = "https://example.com/reason/not-enough-time" (like the example in the draft[56]). On a VSTATUS, reason might store a code link for failure cause. - user.substate: on VSTATUS (and only inside VSTATUS in the draft context - VSTATUS is the only component that takes SUBSTATE property[57]). Possibly in the future if a participant itself had a sub-state, but not defined as such. So likely we only see SUBSTATE within VSTATUS. Therefore, the .status files in StatusHistory will have a substate attr if applicable. The allowed values, per draft, include "OK", "ERROR", "SUSPENDED"[11]. We can encourage those values if user sets it (e.g., via documentation or a validation script). - user.percent-complete: already supported in VTODO (we can have that on a task file). And now, also on a participant file (like Alice is 20% done with her part). We would then include PERCENT-COMPLETE property inside the PARTICIPANT component. Setting that xattr on alice.participant file would do it.
All these are straight-forward additions. They don't require new structural changes, just acknowledging that these fields exist and making them accessible via xattrs. The heavy lifting is done in the ICS generation: Agendafs must ensure to include these in the ICS output and parse input ICS to populate these xattrs. For instance, if a server sends an updated ESTIMATED-DURATION, Agendafs should update the user.estimated_duration xattr accordingly.
Example usage integration: - After creating a task, user sets an estimated duration:
setfattr -n user.estimated_duration -v "PT30M" "ProjectTasks/Buy Groceries.todo"
This indicates about 30 minutes needed for groceries shopping. A script or user could later sum durations of all tasks to plan their day. - If a participant declines the task and gives a reason code, the user (or the syncing client) might set:
setfattr -n user.partstat -v "DECLINED" "Plan Event/Participants/bob.participant"
setfattr -n user.reason -v "https://example.com/reason/overbooked" "Plan Event/Participants/bob.participant"
Which adds a reason link explaining Bob is overbooked. - A task in progress encountering an issue: The user could log a VSTATUS:
STATUS_FILE="Plan Event/StatusHistory/2025-12-05T15:00.status"
touch "$STATUS_FILE"
setfattr -n user.status -v "IN-PROCESS" "$STATUS_FILE"
setfattr -n user.substate -v "SUSPENDED" "$STATUS_FILE"
setfattr -n user.reason -v "https://internal.tracker/issue/123" "$STATUS_FILE"
This would create a status entry noting that as of Dec 5, the task is still in-process but suspended, and link a tracker issue 123 as the reason (maybe an issue that caused the suspension).
This richly documents the task's state.
New Participant Status: "FAILED" for To-dos
The draft extends the allowed values for the PARTSTAT (participation status) of a VTODO to include "FAILED"[17][58]. This means an attendee/participant can explicitly indicate they could not complete their part of the task. In older specs, tasks didn't have a FAILED partstat (only overall task status had an implicit concept of failure if not completed by due date, but participants were just accepted/declined/in-process etc.). Now, if a participant fails, that can trigger things (especially with TASK-MODE Automatic-Failure, where one participant failing the task could mark the whole task failed automatically[44]).
In our system, this just means user.partstat xattr on participant files can now be set to "FAILED" as well. We should ensure that when user wants to mark a participant failed, they use the exact token "FAILED" (all caps) to comply with standard tokens (case-insensitive probably but usually stored uppercase). The example given in the draft:
ATTENDEE;REASON="https://example.com/reason/not-enough-time";
 PARTSTAT=FAILED:mailto:jsmith@example.com
```[56].
In our representation, that corresponds to a PARTICIPANT with partstat FAILED and a reason URI. 

So the main incorporation here is acknowledging that value as valid and maybe using it in logic: e.g., if all participants partstat = FAILED, likely overall task is FAILED. We can't rely on server if server doesn't implement, but maybe we could have a script to detect that and set task status accordingly if no one completed it. However, since we might have TASK-MODE to do it, we might not implement such logic in the filesystem. It's fine to let the user or server handle that.

### Relationships and Identifiers (LINK, REFID, etc.)  
Beyond the core features explicitly listed in the user query, the context of the draft and related RFCs (like RFC 9253) introduces `LINK`, `REFID`, and improved `RELATED-TO`. We already integrated `RELATED-TO` for parent/child and dependencies in our design (symlinks, directories). We should briefly mention how **REFID** and **LINK** might be utilized:
- **REFID**: A property to group tasks by an arbitrary key[59]. E.g., multiple tasks having `REFID:XYZ123` means they all relate to something (project code, case number). The draft suggests using it to group tasks by a key (like all tasks related to "Manhattan" project might have REFID "Manhattan")[59]. We could expose REFID as `user.refid` xattr on tasks. If a user sets the same refid on several tasks, that is a way of tagging/grouping too (like categories but an explicit single-valued key). We might not have a UI mechanism beyond that, but one could query it. Or we could create a virtual directory aggregating by refid similarly to categories if desired.
- **LINK**: A more general way to link to external resources or even other ICS components (via URI or UID reference)[24]. For example, a task might have a `LINK` property pointing to a URL of a document or to another calendar entry's UID with a special URI scheme. We can allow a user to add `user.link` attributes (maybe multiple allowed, though xattr usually each name is unique unless you index them, but one could have multiple LINK lines - perhaps we'd number them like `user.link[1]`, `user.link[2]` using some scheme if needed). Or simpler: allow a single link, and if multiple needed, user could comma-separate or something. Since this is an edge case, we'll not focus heavily. But for completeness, if the user needed to attach a link (say a Google Doc for task details), they could do:
  ```bash
  setfattr -n user.link -v "https://docs.example.com/plan-details" "Plan Event.todo"
  ```
  This would add a `LINK:` property in ICS.

These features complement linking and grouping. They might not be heavily used by a user manually, but since jtxBoard is all about structured data, including them ensures no loss of information if some tasks come with those fields (e.g., from an enterprise system or something).

**Summary of ICS Draft Integration:**  
By adding support for PARTICIPANT, VSTATUS, TASK-MODE, and the new properties, our Agendafs-based system can capture a far more detailed picture of task management:
- Multi-person task assignments with per-person status and progress.
- Automated workflow rules for completion/failure.
- Rich status histories and reasons, enabling traceability.
- Additional scheduling info like estimated effort for better planning.

We carefully store each of these in a place (file or attribute) that makes sense in the filesystem hierarchy, ensuring the user can access or modify them easily. The next sections will show how a user actually interacts with these features in practice using Bash/TUI workflows, but before that, we discuss how we replace the previous application logic with this filesystem approach in the specific deployment context (Debian 11 on ARM64).

## Replacing Kotlin Logic with Agendafs on Debian 11 (ARM64)  
With the new design outlined, we now focus on the **deployment and migration** aspect: how to replace the prior Kotlin-based jtxBoard logic layer with Agendafs acting as the core, on a target system which is a Debian 11 Linux environment running on ARM64 (such as devices with an RK3588 SoC). This involves both setting up Agendafs on that platform and ensuring all the functionality previously implemented in Kotlin is now handled either by Agendafs, by our shell workflows, or by external tools.

### Deployment of Agendafs on Debian 11 (ARM64)  
Agendafs is implemented in C (with source files like `fuse_node.c` and `ical_extra.c` mentioned), or possibly a combination of C and Python (if it used libical via bindings, but likely pure C). We will assume it's open-source (the developer posted about it on sourcehut), and we can compile it for ARM64. Debian 11 has the necessary build tools and libraries (we might need libfuse, and possibly libical or something if not statically done).

**Steps to install:**
1. **Install FUSE:** Ensure the `fuse3` package (or fuse2 if it uses older API) is installed on Debian. This provides the kernel module and user-space library for FUSE. Debian 11 likely includes FUSE by default in the kernel and `libfuse3-3` as library.
2. **Compile Agendafs:** Since it's not an official package yet, we clone the repository (as of writing, the source is on sr.ht as `~marcc/agendafs`). On the ARM64 device, with developer tools (`build-essential` etc.), run the `make` or compile instructions provided. There might be dependencies:
   - Possibly uses `libical` for parsing ICS? If so, install `libical-dev`. Alternatively, it might have its own ICS parsing (`ical_extra.c` suggests some custom handling).
   - Uses `libuuid` or similar for generating UIDs (if needed).
   - Standard libc and fuse libraries.
   The compilation should produce an executable (perhaps called `agendafs`).
3. **Create a mount point:** e.g., `/mnt/agendafs`.
4. **Configure vdir storage:** Agendafs likely needs a directory of ICS files to operate on, or it can connect to CalDAV directly. Based on earlier info, it synchronizes "through vdir storage"[18], meaning it expects local .ics files. We will set up vdirsyncer to maintain a local directory (e.g., `~/.local/share/vdirsyncer/jtxboard_tasks/`) containing ICS files for our tasks and journals. Each ICS file corresponds to one item. vdirsyncer typically names them with random UID-based filenames by default.
5. **Launch Agendafs:** Run something like:
   ```bash
   agendafs -o src=~/.local/share/vdirsyncer/jtxboard_tasks/,mode=tasks /mnt/agendafs/ProjectTasks
   ```
   (This is speculative CLI: we assume agendafs has options to point to a directory of .ics and maybe a mode or config for how to treat them. It might also read a config file for collections. If multiple collections (journal, tasks) are needed, possibly run multiple instances at different mount subpoints, or if Agendafs supports multiple in one go.)
   If agendafs is configured via a config file, we'll prepare that, specifying the path to the local ICS folder and maybe telling which component types to expect. Perhaps it automatically figures out by reading the ICS content.
6. **Verify mount:** After mounting, `ls /mnt/agendafs/ProjectTasks` should show our tasks structured as described. If none exist yet, it might be empty or allow adding.

If jtxBoard's data already exists on Nextcloud or similar, the initial sync via vdirsyncer will populate the local directory with .ics files. Agendafs will then present them. The good news is, because both jtxBoard and our system use the same data model (ICS), the migration is mostly just switching the interface: from a Kotlin app to a filesystem. We should ensure any ICS extensions jtxBoard used are not lost:
- jtxBoard uses VJOURNAL for notes and VTODO for tasks[26]. Those are standard. It might not have used Participant or VSTATUS because those are new, but it may use RELATED-TO to link tasks and journals (some apps did linking by using RELATED-TO with custom RELTYPE like "X-LINKED"). If so, our system will preserve them (if unknown reltype, stored anyway).
- jtxBoard likely used CATEGORIES for tags, which we fully support via xattr.

So the data continuity is preserved.

**Performance on ARM64 (RK3588):** The RK3588 is a powerful SoC with multiple cores, and FUSE overhead for a handful of calendar files is negligible. Even with thousands of items, it's mostly idle until you access something. So performance is not a concern. Extended attributes calls might be slightly slower than in-memory variables, but again trivial in context (plus FUSE can cache some attribute data if coded to).

One thing to configure is **auto-start**: mount the FS at login or boot (perhaps via an entry in `/etc/fstab` using fuse with user option, or a systemd service to run agendafs at user session start). For safety, one might choose not to mount all the time, but mount when needed. However, a user likely wants it active so that any scripts (like remind notifications or simply their shell alias for quick note-taking) will find it.

**Replacing Kotlin logic:** The Kotlin logic likely handled:
- Data storage (which is now ICS files via vdirsyncer, covered by our FS).
- Business logic like recurring tasks (e.g., generating a new occurrence after one is done), sorting tasks, and providing UI forms for editing.
- Possibly local notifications or integrations.

We replace storage with ICS + FS (done). Business logic:
- **Recurring tasks**: ICS can have RRULE for recurrence even for VTODO (some clients and servers support repeating tasks). jtxBoard supports recurring tasks[60][61]. If jtxBoard used RRULE in the ICS (likely yes), we must be able to interpret and handle it. A recurring VTODO means once completed, the next occurrence should appear. However, handling recurrence for tasks is tricky as not all servers or apps auto-generate the next instance (some treat recurring tasks differently than events).
  - Many CalDAV servers handle recurring tasks by keeping one master and generating instances or updating the due date after completion. 
  - If not, jtxBoard might have been doing it manually (maybe on marking complete, it duplicates the task with new dates).

In our environment, we do not have a built-in recurring event generation logic in Agendafs (as far as we know). If none, we can implement recurrence management using shell scripts:
  - For example, a daily cron that runs a script to check tasks with RRULE that are completed and create the next occurrence. Or maybe easier, rely on the user to manually mark complete and then copy the task file to new date. But that's not user-friendly.

Alternatively, encourage using the CalDAV server's recurrence if supported. Some servers (like Nextcloud's tasks) do support recurring tasks - marking one completed will generate the next. We should test that or assume modern CalDAV servers do handle it to some extent. Given the time, we can at least store the RRULE and trust the environment or manual steps.

But since the blueprint should be comprehensive, let's propose a simple approach: after marking a recurring task done, the user can run a command (or an automatic hook triggers) that increments to next occurrence:
  - e.g., `taskrecur Tool` or simply instruct: "to generate next occurrence, copy the .todo file, adjusting the dates according to rule."

We'll cover this in workflows more concretely.

- **Sorting and UI**: The Kotlin app would show tasks sorted or filtered. In our world, sorting can be done with `ls` or custom `find | sort` queries. TUI programs like `nnn` (terminal file manager) can help navigate. If we want a quick TUI list of tasks by status, we could write a small script that prints tasks with relevant info (by reading xattrs and content). That might not be in initial deployment but possible. Many CLI to-do managers exist; however, mixing with ICS is unique, so customizing might be needed.

- **Notifications**: If the Kotlin app sent reminders for due tasks, we can replicate that by using `at` or `cron` jobs reading the `user.due` xattrs. For example, one could write a daily cron job to find tasks due in next day and use `notify-send` (desktop notification) or send email. Not strictly necessary for the blueprint but worth mentioning as integration.

In short, the **Kotlin app's responsibilities are now distributed** between:
- Agendafs (data handling, adding ICS structure like participants and linking).
- vdirsyncer (sync).
- Shell scripts and the user (for logic like marking complete and generating recurrences, or sending notifications).
- Possibly underlying CalDAV server (which might take on some logic, e.g., automatic status changes if using TASK-MODE, or recurrence expansion).

One must ensure nothing critical is lost:
For example, if jtxBoard had a feature to link a journal entry to a task, that likely was stored as RELATED-TO in ICS (maybe the journal's UID in the task's RELATED-TO or vice versa). We support that linking via our structure (the user can set it up or at least view it if it's present). If it was an internal reference (less likely), we would add it as ICS manually.

**Memory/Resource usage:** On an ARM64 board, FUSE and parsing ICS for a few hundred items is lightweight (couple of MB of RAM). The Kotlin app likely consumed more memory on Android device than this will on a server.

So, replacing the Kotlin layer is feasible and essentially complete once Agendafs is running with the ICS data. The user will interact via file operations rather than a GUI.

Next, we will demonstrate exactly how those interactions look with **Bash and TUI workflow examples**, effectively showing how all the pieces (Agendafs's architecture from above, and the new ICS features) come together for practical usage.

## Bash/TUI Workflows for Calendar and Task Management  
In this section, we present a series of practical workflows that a user (or system administrator) can perform on the Agendafs-mounted filesystem to achieve the same outcomes that jtxBoard's UI provided. We cover creating and editing journal entries, managing tasks and subtasks, linking items, tagging, setting recurrence, updating statuses (including participants' statuses), and monitoring progress. Each workflow is illustrated with Bash commands or TUI interactions, along with explanations of how those translate into underlying ICS changes via Agendafs.

These examples assume the mount point is at `/mnt/agendafs` with subdirectories as described (for example, `ProjectTasks` for tasks, `PersonalJournal` for journals). Adjust paths as per actual configuration.

### 1. Creating a Journal Entry (VJOURNAL)  
**Scenario:** The user wants to jot down notes or a diary entry for today (or any date) - similar to using jtxBoard to add a journal entry.

**Filesystem Workflow:**  
- Navigate to the Journal directory (e.g., `PersonalJournal`). Within it, either create a new file named with today's date or open an editor directly on that filename.

Example using the shell to create a quick entry for Dec 1, 2025:
```bash
cd /mnt/agendafs/PersonalJournal
# Create a new journal entry for today:
echo "Started working on the Agendafs integration blueprint. Completed the design section." > "2025-12-01.txt"
The above command creates a file 2025-12-01.txt and writes the content in one go. Alternatively:
nano 2025-12-01.txt
and then type the content in the editor, save and exit.
* Optionally, set or verify metadata. Perhaps we want to tag this entry or ensure it's dated:
* By default, Agendafs might infer the date from the filename "2025-12-01". If not, we can explicitly set the start date (DTSTART) via xattr:
  setfattr -n user.dtstart -v "20251201" "2025-12-01.txt"
  This ensures the ICS VJOURNAL gets a DTSTART of 2025-12-01. For all-day journal entries, date format without time is used (which ICS treats as all-day).
* Add a category/tag if desired:
  setfattr -n user.categories -v "Work,Documentation" "2025-12-01.txt"
  This will add CATEGORIES:Work,Documentation to that journal entry, effectively tagging it.
* The name of the file (2025-12-01) might automatically serve as the SUMMARY (title) of the journal in ICS. If not, we could set an explicit summary:
  setfattr -n user.summary -v "Agendafs Integration Work" "2025-12-01.txt"
  But typically for diary, summary might be optional or just the date.
Result: The file 2025-12-01.txt now exists with the content as its body (ICS DESCRIPTION). Agendafs translates this into a VJOURNAL entry behind the scenes:
BEGIN:VJOURNAL
UID:... (auto-generated or derived)
DTSTART;VALUE=DATE:20251201
SUMMARY:Agendafs Integration Work
CATEGORIES:Work,Documentation
DESCRIPTION:Started working on the Agendafs integration blueprint. Completed the design section.
END:VJOURNAL
(The actual ICS assembly is done by Agendafs, but it would resemble the above.)
From the user perspective in the TUI, if they list the directory:
$ ls -l /mnt/agendafs/PersonalJournal
-rw-r--r-- 1 user user   ... "2025-12-01.txt"
They see their entry. They can open it anytime with an editor to read or append more notes (since diaries might be appended to later in the day). The extended attributes can be viewed:
$ getfattr -d "2025-12-01.txt"
# file: 2025-12-01.txt
user.categories="Work,Documentation"
user.dtstart="20251201"
user.summary="Agendafs Integration Work"
This confirms metadata.
Comparison to jtxBoard: In jtxBoard's app, the user would tap "new journal", type text, maybe assign categories. We achieved the same with a text editor and one optional xattr command. The content and metadata are properly stored in ICS format. This entry will sync via vdirsyncer to the server (as an .ics file in the journal calendar collection).
2. Creating a To-do Task (VTODO)
Scenario: The user wants to add a new task "Plan Event" with a due date and some details.
Filesystem Workflow:
Navigate to the tasks directory (ProjectTasks in our example):
cd /mnt/agendafs/ProjectTasks
Create a file for the task. Because "Plan Event" is likely to be a project with subtasks, we might create it as a directory so that we can add subtasks under it later. If we create it as a file first, we can convert to directory later if needed, but let's show the direct way:
mkdir "Plan Event"
# Create a summary/description file for Plan Event
echo "This is a task to plan the annual conference event." > "Plan Event/Details.txt"
We chose to have a Details.txt inside the directory for the main task's notes. Another approach: create "Plan Event.todo" as a file first:
echo "Plan the annual conference event." > "Plan Event.todo"
and then later if we want subtasks, we rename it into a directory. However, let's continue with the directory model for clarity:
Set task metadata via xattrs on the directory or the details file: - If Agendafs treats the directory as the task container, possibly the task's properties should be set on the directory itself or on a special file representing the task (like the Details.txt). For simplicity, we will assume setting on the directory works (Agendafs might allow xattrs on directories too, which it can map to the containing VTODO).
# Set summary of the task (title)
setfattr -n user.summary -v "Plan Event" "Plan Event"
# Set due date (assuming an example due date of Dec 31, 2025 5pm UTC)
setfattr -n user.due -v "20251231T170000Z" "Plan Event"
# Set status to NEEDS-ACTION (which is default, so this might be implicit)
setfattr -n user.status -v "NEEDS-ACTION" "Plan Event"
# Set priority if desired (e.g., 1-high, 5-medium, etc. ICS uses 1-9)
setfattr -n user.priority -v "5" "Plan Event"
(If setting xattr on directory doesn't work due to how FUSE implemented, we might set them on the Details.txt or create a file .meta to hold them. But let's assume directory xattr is possible for conceptual coherence.)
The due date is now set. We can also set an estimated duration:
setfattr -n user.estimated_duration -v "PT8H" "Plan Event"
(say we expect planning to cumulatively take 8 hours of work).
Check attributes:
$ getfattr -d "Plan Event"
user.due="20251231T170000Z"
user.estimated_duration="PT8H"
user.priority="5"
user.status="NEEDS-ACTION"
user.summary="Plan Event"
Now, the task exists with initial metadata. The content "This is a task to plan the annual conference event." is stored in Details.txt which presumably Agendafs will merge as DESCRIPTION in the VTODO.
Add subtasks (linked tasks): Since "Plan Event" likely involves multiple steps, we add them as children:
cd "Plan Event"
# Create subtask 1:
echo "Research venues and costs." > "Book Venue.todo"
setfattr -n user.summary -v "Book Venue" "Book Venue.todo"
setfattr -n user.status -v "NEEDS-ACTION" "Book Venue.todo"
# Suppose this subtask has an earlier deadline (e.g., Dec 15)
setfattr -n user.due -v "20251215T170000Z" "Book Venue.todo"

# Create subtask 2:
echo "Send invitations to participants." > "Invite Participants.todo"
setfattr -n user.summary -v "Invite Participants" "Invite Participants.todo"
setfattr -n user.status -v "NEEDS-ACTION" "Invite Participants.todo"
setfattr -n user.due -v "20251220T170000Z" "Invite Participants.todo"
When we created these .todo files inside the Plan Event directory, Agendafs will interpret that as creating two new VTODOs that are children of Plan Event's VTODO. It should automatically add the RELATED-TO relationship. If it doesn't automatically, the act of placing them in that directory implies it, so internally Agendafs should assign: - For Book Venue: RELATED-TO:<UID of Plan Event>;RELTYPE=CHILD[12]. - For Invite Participants: similarly. And possibly Plan Event itself could get references to them, but not required.
We have effectively linked the tasks by hierarchy. The parent task "Plan Event" is our big task, and two subtasks are clearly under it in the file tree.
Viewing and editing tasks: The user can use ls to view tasks:
$ ls /mnt/agendafs/ProjectTasks
Plan Event/   Buy Groceries.todo   ...
$ ls /mnt/agendafs/ProjectTasks/"Plan Event"
Book Venue.todo   Invite Participants.todo   Details.txt   Participants/
(Participants/ might exist empty now or not until we add participants.)
To work on a task or update it, open its file:
vim /mnt/agendafs/ProjectTasks/"Plan Event/Book Venue.todo"
This shows "Research venues and costs." which the user can edit to add maybe details or a checklist in the description. Save changes.
Marking subtask as done when finished:
setfattr -n user.status -v "COMPLETED" "Book Venue.todo"
setfattr -n user.completed -v "$(date -Iseconds)" "Book Venue.todo"
The user.completed attribute (if Agendafs provides it) could store the COMPLETED timestamp (ICS COMPLETED property) when the task was finished, which is good practice. We manually set it to now (date in ISO8601 format which ICS will parse to DATE-TIME). This records the completion time in ICS.
Assigning participants to a task: Perhaps we want to assign "Plan Event" to Alice and have Bob as an observer:
mkdir "Plan Event/Participants"
touch "Plan Event/Participants/alice.participant"
setfattr -n user.email -v "alice@example.com" "Plan Event/Participants/alice.participant"
setfattr -n user.role -v "REQ-PARTICIPANT" "Plan Event/Participants/alice.participant"
setfattr -n user.partstat -v "NEEDS-ACTION" "Plan Event/Participants/alice.participant"

touch "Plan Event/Participants/bob.participant"
setfattr -n user.email -v "bob@example.com" "Plan Event/Participants/bob.participant"
setfattr -n user.role -v "NON-PARTICIPANT" "Plan Event/Participants/bob.participant"
setfattr -n user.partstat -v "ACCEPTED" "Plan Event/Participants/bob.participant"
Now Alice is the assigned person (required participant) and Bob is just observing (non-participant role). Bob's partstat accepted just means he's aware. Alice's needs-action means she hasn't accepted yet. If Alice later accepts (maybe through another interface or we simulate it):
setfattr -n user.partstat -v "ACCEPTED" "Plan Event/Participants/alice.participant"
Now, if our server and environment fully implemented iTIP, Alice would likely accept via an email or CalDAV client which would sync back. But since we are controlling both ends, we just set it directly.
Using TASK-MODE: Let's say for "Plan Event", we want the server to auto-complete it when Alice marks completed. So:
setfattr -n user.task_mode -v "AUTOMATIC-COMPLETION" "Plan Event"
This sets TASK-MODE to automatic-completion. According to spec, when all participants (in this case just Alice, since Bob is observer) have partstat COMPLETED, the server should mark the whole task done[6]. If we also want the due-date auto-failure behavior:
setfattr -n user.task_mode -v "AUTOMATIC" "Plan Event"
(Automatic covers both completion and failure according to the draft[46]).
Given we set to automatic-completion for now, the expectation is: once Alice's participant status goes to COMPLETED, the server will flip Plan Event's STATUS to COMPLETED.
We can test scenario manually: - When Alice finishes her task part:
setfattr -n user.partstat -v "COMPLETED" "Plan Event/Participants/alice.participant"
Possibly also set a user.percent-complete=100 for Alice or something. At sync time, the server sees all participants done, sets Plan Event status to COMPLETED, and perhaps adds a VSTATUS entry: (Maybe not all servers do, but some might log that). If it does, we would see a new file appear in Plan Event/StatusHistory after next sync (if Agendafs monitors underlying ICS changes). Or if not, at least the Plan Event user.status would become COMPLETED after sync.
Recurring tasks: Suppose we have a recurring task like "Backup Database" that repeats weekly. jtxBoard supports recurring tasks[61]. How to do that: - Create the task as usual:
echo "Perform weekly backup" > "Backup Database.todo"
setfattr -n user.summary -v "Backup Database" "Backup Database.todo"
setfattr -n user.rrule -v "FREQ=WEEKLY;BYDAY=MO" "Backup Database.todo"
(We assume Agendafs can interpret an xattr user.rrule to add an RRULE property. If not built-in, user could manually add via an ICS injection, but xattr approach is fine if implemented). - Set a start date and possibly due date for each occurrence:
setfattr -n user.dtstart -v "20251206T090000Z" "Backup Database.todo"
setfattr -n user.due -v "20251206T100000Z" "Backup Database.todo"
That sets the first occurrence on 6 Dec 2025, due by an hour later, repeating every Monday.
Now, ICS will treat that as one recurring VTODO. Many clients (and perhaps server) handle recurring VTODO by generating occurrences as separate instances on the fly. If our server doesn't, we might handle it: - When 6 Dec passes or when we mark that occurrence done, we might need to generate an EXDATE or something and update for next recurrence. But ICS tasks recurrence handling is not as uniform as events. However, as a blueprint, we ensure the data is there so that at least user's other CalDAV clients see it properly.
Completing a recurring task: If we mark "Backup Database" completed on that Monday:
setfattr -n user.status -v "COMPLETED" "Backup Database.todo"
setfattr -n user.completed -v "20251206T093000Z" "Backup Database.todo"
What now? According to RFC, if a recurring to-do has an RRULE, completing one doesn't automatically generate the next; it basically would consider the entire series done. The draft doesn't change that logic as far as I know. Some systems handle recurring tasks by creating separate tasks for each recurrence upon completion. jtxBoard likely had logic: when one instance is completed, create the next occurrence as a new VTODO (possibly with a new UID). Without the Kotlin app, we could emulate this: - We detect that "Backup Database.todo" was a recurring task and got completed. We then manually or via script duplicate it for the next occurrence: - Compute next Monday's date (say 13 Dec 2025). - Create a new "Backup Database 2.todo" or perhaps better: adjust the current one's dates to next occurrence and mark it incomplete again, while logging the past completion in VSTATUS.
Alternatively, use the CalDAV server's support if any. Some CalDAV treat recurring VTODO as always one task; you mark complete, it generates the next occurrence by bumping DTSTART/DUE to the next. But not sure if that's standard. Possibly the ical-tasks-16 draft or earlier CalDAV specs had something about RRULE on VTODO meaning repeating. If none, we might just break it into separate tasks for each week in practice.
For completeness, we can propose a small script advance_recur.sh:
#!/bin/bash
TASK="$1"
NEXTDATE="$2"
# TASK is path to .todo file of recurring task, NEXTDATE is next occurrence date/time in format YYYYMMDDThhmmssZ
# Extract RRULE
RRULE=$(getfattr -n user.rrule --only-values "$TASK" 2>/dev/null)
if [ -z "$RRULE" ]; then
  echo "Not a recurring task or RRULE not found."
  exit 1
fi
# Mark current as completed if not already
setfattr -n user.status -v "COMPLETED" "$TASK"
setfattr -n user.completed -v "$(date -Iseconds)" "$TASK"
# Now clone the task for next occurrence
# We assume the summary stays same, also other properties.
SUMMARY=$(getfattr -n user.summary --only-values "$TASK")
DESCRIPTION=$(cat "$TASK")  # read file content as description
UID=$(getfattr -n user.uid --only-values "$TASK")  # original UID
NEWUID=$(uuidgen)  # generate new UID for next
# Create a new file for next occurrence
NEXTFILE="${TASK%.*}-next.todo"
echo "$DESCRIPTION" > "$NEXTFILE"
setfattr -n user.summary -v "$SUMMARY" "$NEXTFILE"
setfattr -n user.status -v "NEEDS-ACTION" "$NEXTFILE"
setfattr -n user.dtstart -v "$NEXTDATE" "$NEXTFILE"
# Compute due by adding whatever interval (assuming same duration, or get from estimated_duration if present)
DUR=$(getfattr -n user.estimated_duration --only-values "$TASK")
if [ -n "$DUR" ]; then
  # If an estimated duration like PT1H, we can attempt to compute due = nextdate + dur
  # (We might need a date utility here; skipping actual sum for brevity)
  :
fi
# Copy priority, categories if any
PRIORITY=$(getfattr -n user.priority --only-values "$TASK"); [ -n "$PRIORITY" ] && setfattr -n user.priority -v "$PRIORITY" "$NEXTFILE"
CATEGORIES=$(getfattr -n user.categories --only-values "$TASK"); [ -n "$CATEGORIES" ] && setfattr -n user.categories -v "$CATEGORIES" "$NEXTFILE"
# For recurring tasks, it might not need an RRULE on the new one (the recurrence was essentially broken into series)
# Or we could keep the same RRULE if we want that to continue indefinitely and not treat them separate. But ICS recurrence for tasks is ambiguous.
This script (conceptual) would complete the current and spawn the next. The complexity shows why one might avoid heavy recurrence with tasks or rely on server or user scheduling.
Due to length, let's not delve deeper; suffice to say, recurring tasks can be managed either by ICS itself (if the server or client supports it elegantly) or by scripting.
3. Linking Notes and Tasks
Scenario: We have a note (journal entry) that is related to a task (for example, meeting minutes note that corresponds to a follow-up task). In jtxBoard, one could link a journal to a task. ICS would do this by a RELATED-TO property cross-linking the VJOURNAL and VTODO (likely with some RELTYPE or just generic).
Filesystem Workflow:
Let's say we have a journal entry 2025-12-01.txt and a task Plan Event. We want to indicate the journal contains context for the task.
One way is to use the LINK property, but better is RELATED-TO. We can add a related-to UID reference: - Get the UID of Plan Event:
UID=$(getfattr -n user.uid --only-values "/mnt/agendafs/ProjectTasks/Plan Event")
- Then set it on the journal:
setfattr -n user.related_to -v "$UID" "/mnt/agendafs/PersonalJournal/2025-12-01.txt"
If RELTYPE needs specification, maybe separate attr like user.related_to_reltype = "CHILD" or some appropriate relationship. However, likely we can just tag it. Alternatively, if Agendafs supports symlinks for linking: We could create a symlink in the journal entry's directory linking to the task, or vice versa: e.g.,
ln -s "../../../PersonalJournal/2025-12-01.txt" "/mnt/agendafs/ProjectTasks/Plan Event/links/2025-12-01.note"
This would be a bit unnatural though. Perhaps easier: add a mention in the journal content referring to the task.
Given ICS offers RELATED-TO, we use the xattr method. After that: getfattr -n user.related_to -d "2025-12-01.txt" might show something like:
user.related_to="UID-of-PlanEvent"
We might also on the task set a reference back to the journal's UID (though not necessary if one-way is enough):
JUID=$(getfattr -n user.uid --only-values "/mnt/agendafs/PersonalJournal/2025-12-01.txt")
setfattr -n user.related_to -v "$JUID" "/mnt/agendafs/ProjectTasks/Plan Event"
Now they are mutually linked.
What this accomplishes is that if a client is aware, it could navigate between them. In our FS, it doesn't automatically create any new symlink, but we can find relation by reading the attributes: One could imagine a fuse feature to show a virtual file with all related items, but not implemented currently. Instead, the user can manually find "which note is related to Plan Event" by searching for its UID in files: e.g.,
grep -R "$(getfattr -n user.uid --only-values 'Plan Event')" /mnt/agendafs/PersonalJournal
This would find any mention (in content or xattr dumps via special means if we could grep xattrs directly, maybe not easily done with normal grep, we'd need to use getfattr on all and search). Or maintain a simple log externally.
Anyway, linking tasks and notes is possible and preserved in the ICS.
4. Updating Status and Progress
We already covered some status updates like completing tasks. Let's demonstrate a complex scenario with participants and VSTATUS: Scenario: During "Plan Event", something goes wrong (no venue available). We want to mark the task as failed and give a reason.
Workflow:
- Mark the task status as FAILED:
setfattr -n user.status -v "FAILED" "/mnt/agendafs/ProjectTasks/Plan Event"
- Add a VSTATUS log entry:
mkdir "/mnt/agendafs/ProjectTasks/Plan Event/StatusHistory"
touch "/mnt/agendafs/ProjectTasks/Plan Event/StatusHistory/2025-12-20T10:00.status"
setfattr -n user.status -v "FAILED" "/mnt/agendafs/ProjectTasks/Plan Event/StatusHistory/2025-12-20T10:00.status"
setfattr -n user.substate -v "ERROR" "/mnt/agendafs/ProjectTasks/Plan Event/StatusHistory/2025-12-20T10:00.status"
setfattr -n user.reason -v "https://example.com/reason/no-venue" "/mnt/agendafs/ProjectTasks/Plan Event/StatusHistory/2025-12-20T10:00.status"
Essentially, we log that at Dec 20, 2025, status went to FAILED, substate error, reason "no venue" (the URL might point to a more detailed explanation). - Mark participant statuses appropriately: maybe Alice's partstat remains NEEDS-ACTION (since she never completed, we can optionally mark her FAILED too):
setfattr -n user.partstat -v "FAILED" "Plan Event/Participants/alice.participant"
setfattr -n user.reason -v "https://example.com/reason/no-venue" "Plan Event/Participants/alice.participant"
Bob as observer might still be ACCEPTED (he was just observing).
In ICS, the presence of a VSTATUS with FAILED and a participant with PARTSTAT=FAILED clarifies that the task didn't complete successfully. If we had TASK-MODE=AUTOMATIC-FAILURE on, the server might have done this automatically when due passed or someone failed. If not, we did manually.
Progress tracking:
Using PERCENT-COMPLETE: - If a task is mid-way, say "Invite Participants" is started by Alice. She can update:
setfattr -n user.status -v "IN-PROCESS" "Plan Event/Invite Participants.todo"
setfattr -n user.percent_complete -v "50" "Plan Event/Invite Participants.todo"
So now that subtask is marked in process and 50% done. If we want to note why it's not 100%: We might not have a reason property outside VSTATUS, but we can add a VSTATUS for the in-process:
touch "Plan Event/Invite Participants.status"  # might have a different scheme for subtask immediate status
Actually, better: use xattrs: Actually ICS doesn't have substate directly on a task's main status except via VSTATUS. So perhaps we skip substate until final. Or create a VSTATUS even for an intermediate state if needed.
Tagging tasks:
If we want to tag tasks (like label Plan Event as "Work"), we set user.categories = Work on it (we did earlier perhaps). We can filter tasks by category easily with a find or by looking into those tag directories if we made them.
Using TUI tools:
While the above is heavy on the command-line detail, in practice one could use a terminal file manager like nnn or mc (Midnight Commander) to navigate and perform many of these actions more interactively: - Navigate through directories to find the task or note. - Press a key to edit (which opens in an editor). - Maybe use built-in actions or custom scripts to set xattrs (some file managers allow running external commands on files). For example, one could configure nnn to have a shortcut that marks the selected file as done by internally calling setfattr.
For instance, a nnn plugin could be created, say pressing "x" on a task opens a small prompt for status (Completed/Cancelled etc.) and then calls setfattr accordingly. This is outside of Agendafs scope but easily done with shell scripts integrated with the TUI.
Another tool: taskwarrior or todoman can import from ICS, but using them directly might bypass the ICS, which we don't want. Instead, our system itself is like a specialized "task" manager but using FS.
shell integration examples: - Add a task quickly: We could create a bash alias or function:
newtask() { 
    local title="$1"; 
    local file="/mnt/agendafs/ProjectTasks/$title.todo";
    echo "" > "$file";
    setfattr -n user.summary -v "$title" "$file";
    setfattr -n user.status -v "NEEDS-ACTION" "$file";
    echo "Task '$title' created.";
}
Then newtask "Call Vendor" adds a blank task.
* List all tasks pending: One could run:
  find /mnt/agendafs/ProjectTasks -name "*.todo" -exec getfattr -n user.status --only-values {} + | grep -l "NEEDS-ACTION"
  (Though getfattr prints values, not easily tied to name. Instead, use a loop:
  for f in $(find /mnt/agendafs/ProjectTasks -name "*.todo"); do
  status=$(getfattr -n user.status --only-values "$f" 2>/dev/null)
  if [ "$status" = "NEEDS-ACTION" ]; then echo "$f"; fi
done
  This prints all not yet done tasks.)
We can format output nicely or even use dialog or fzf to interactively pick tasks. The possibilities in shell are endless. The blueprint's main duty is to ensure all data is accessible and modifiable, which it is via xattrs and file content.
5. Synchronization and Integration with vdirsyncer & Tools
Finally, after doing local operations, the changes must sync: - The user runs vdirsyncer sync (or it runs automatically periodically via cron). - vdirsyncer reads the ICS files under ~/.local/share/vdirsyncer/jtxboard_tasks/ (which Agendafs populates from our actions) and pushes changes to server, and pulls server changes. If server made changes (like if the user also uses jtxBoard on phone to add something, or if the server auto-completed a task due to TASK-MODE triggers), vdirsyncer brings those down. Agendafs then should reflect them: - If vdirsyncer modifies or adds an .ics in the local storage directory, Agendafs either needs to catch that via file system events or if it doesn't, we might need to unmount and remount or send a SIGHUP to instruct it to reload. Ideally, Agendafs would respond to IN_MODIFY events on the underlying dir (since it could monitor it) and update the FUSE view. If not implemented, the user might occasionally refresh by remounting or triggering an internal refresh (maybe the developer included a command or auto polling). - After sync, the user sees updates: For example, if on mobile jtxBoard marked "Invite Participants" completed, when we sync, the ICS for that task now has STATUS:COMPLETED. Agendafs should then show user.status="COMPLETED" on that file. The user could run a command to find new completions or watch for differences. Possibly the stat mod time on the file might update, which can be a cue that something changed.
Integration with Calendar UIs or scripts:
- If the user uses khal (a CLI calendar) or calcurse, those can read ICS from files. We could configure them to read from the same vdir directory or even directly from the Agendafs mount (though they might not see xattrs, they only care ICS content). - For instance, khal could be pointed to the ICS files for events. For tasks, todoman (another CLI for VTODO) could be used if we point it to ICS collection. Actually todoman by pimutils works with vdir (the same that vdirsyncer uses). However, if we start using todoman to edit tasks, it will operate on ICS files concurrently with Agendafs. That might be okay if both properly coordinate (like locking file access). It's simpler to stick to one interface at a time to avoid confusion.
* Or integrate with notify-send for due tasks: Create a cron job that every day runs:
  for task in $(find /mnt/agendafs/ProjectTasks -name "*.todo"); do
  due=$(getfattr -n user.due --only-values "$task" 2>/dev/null)
  status=$(getfattr -n user.status --only-values "$task" 2>/dev/null)
  summary=$(getfattr -n user.summary --only-values "$task" 2>/dev/null)
  if [ -n "$due" ] && [ "$status" = "NEEDS-ACTION" ]; then
    # If due is near (within 24h)
    if [[ "$(date -u +%Y%m%d%H%M)" > "${due:0:12}" && ... ]]; then
      notify-send "Task Due" "$summary is due by $(echo $due | sed 's/T/ /')" 
    fi
  fi
done
  This pseudo-code checks upcoming deadlines and sends a desktop notification for each. The user thus gets reminded akin to how a mobile app would notify.
Atomicity and Conflict Example:
- Suppose the user and a colleague both edit the same task (the colleague via CalDAV from somewhere else). vdirsyncer might detect a conflict if the ICS was changed on server and locally between syncs. vdirsyncer's default conflict resolution is "the last modified wins" or it creates a conflict file. To avoid that, one should try to not concurrently edit the same item in two places frequently. Our system doesn't have a locking mechanism across devices aside from what CalDAV provides (which is usually last-write-wins with etag checking). If a conflict happens, vdirsyncer might create two versions of ICS in local folder, naming one with .conflict suffix. In that case, Agendafs might present two objects (maybe it will show a duplicate or an error for that ICS). The user or admin would then have to reconcile manually. These scenarios are not unique to our approach - it's inherent to syncing.
* Atomic writes: When we edited content or xattrs, each is an atomic operation. But doing multiple (like changing status and adding VSTATUS directory) is not atomic altogether. If a crash happens in middle, ICS might end up inconsistent (like status changed but no VSTATUS entry). This is minor inconsistency that doesn't break the file format, just may misrepresent the state (lack of an entry). The user can still fix it after. Ideally, one might want a transaction, but ICS doesn't support that; we rely on manual coherence.
Standard Compliance:
We should double-check that none of our manipulations create non-compliant ICS: - xattr to ICS mapping must ensure correct syntax (Agendafs should handle escaping, e.g., colons, commas, etc in values as ICS requires). - The draft features we use (PARTICIPANT, VSTATUS, TASK-MODE, etc.) should be either accepted by server or at least not dropped. Many CalDAV servers might not know about PARTICIPANT and VSTATUS yet (if it's a draft). How will they react? Possibly: - They may allow unknown subcomponents and properties (which good servers do, storing them). - Or might strip them out. If it strips, our data for those features would be lost on sync - that's not ideal. If using Nextcloud Tasks or similar, I suspect it might just ignore and keep them in storage (Nextcloud tends to preserve unknown content). - There is a risk: some servers have fixed schemas and might reject or drop unrecognized lines. The draft even says the server should reply with an error for unsupported TASK-MODE values[48][62]. If our server doesn't support any, it might error. For example, if we set TASK-MODE=AUTOMATIC, and server doesn't support TASK-MODE at all, it might either ignore or explicitly reject. CalDAV's generic behavior for unknown properties is typically to ignore unless there's an extension requiring otherwise. But ical-tasks-16 suggests an error for unsupported mode. If the server implements that logic partially (like Apple Calendar or something in future), then user would find their sync failing for that item (vdirsyncer log will show HTTP 403). If so, the user might have to remove or change that property.
Because our blueprint user specifically wanted these features, presumably they plan to use a server that will support them (maybe a test CalDAV server that's up to spec, or they are comfortable with ignoring some sync issues). We note this as something to monitor. It's an early adoption scenario.
Alternate Models Discussion (Dialects): (We intersperse some alternatives above in reasoning, but here let's summarize briefly)
Our approach chooses: - FUSE FS as interface vs building a separate CLI tool or using an existing CLI tasks manager. We argued FUSE gives flexibility to use any editor or script and piggybacks on user's familiarity with file operations. - Representing subtasks with directories vs a flat list with references. We chose directories to visually group and ease creation of new subtask by just adding a file in a dir. An alternative could have been a flat list where each task has maybe a field listing children, but that would require editing an attribute with multiple UIDs - not as intuitive. - Extended attributes vs encoding metadata in filenames or file content. We opted for xattrs to keep things structured and separate from content. Another approach some systems take is to embed metadata in the first line of the file (like Title: ... or YAML front matter). That would have allowed editing everything in one text flow, but at cost of needing a parser in editor or manual formatting. Xattrs keep it clean and machine-friendly, albeit requiring use of getfattr/setfattr for certain edits. For advanced users, that's fine; or we provide wrapper scripts to simplify common xattr actions (like a due command to set due date easily). - Integration with existing tools: We considered using todoman or taskwarrior. We decided to stick to ICS and not convert to a different format. Taskwarrior, for example, is a popular CLI task manager but has its own database and sync (with taskserver or plain text). Using it would sidestep ICS and break CalDAV sync, so not suitable if we need to remain ICS. Todoman actually directly reads ICS (the vdir format) and is meant to sync with CalDAV (through vdirsyncer). In fact, one could use todoman to list and mark tasks done concurrently with our FS. Todoman would operate on ~/.local/share/vdirsyncer/jtxboard_tasks/ ICS files. If one is comfortable switching between FS and todoman CLI, they could. But using two interfaces could be confusing. Our FS is basically offering similar functionality but in a different paradigm.
Implications of design trade-offs:
One trade-off: complexity vs simplicity. Our file system approach is explicit - everything corresponds to a file or attribute you manipulate. This is powerful but demands knowledge. A more abstract CLI could hide ICS complexity behind commands (like todo --complete 5). However, building a full CLI would replicate features of existing ones, and the user specifically wanted a blueprint leveraging Agendafs, implying they want to use the FS directly.
Another trade-off: Data duplication vs single source. If we had chosen to duplicate tasks into separate simplified text or use an external database for indexing, we could get faster queries or fancy reports. But that would break the single-source-of-truth of ICS. We keep ICS as the one source, ensuring consistency but maybe sacrificing some convenience (like quickly searching xattrs requires commands, whereas a DB or indexing service could be faster). If needed, the user can always index the ICS with external tools (like using recoll or grep and making custom index, or writing small Python to parse ICS into a list).
Security considerations recap:
- Running FUSE requires at least user-level permission (no root needed if configured). On Debian, user may need to be in fuse group or similar. - The data in Agendafs is only as secure as file permissions of the underlying ICS and the mount. We mounted likely as user, accessible to that user. If multi-user system, ensure to not use allow_other mount option unless you want others to see your data. - Extended attributes in user namespace are only modifiable by owner[63], so no other user can tamper unless they can act as root or have appropriate capability (and if the mount doesn't allow others). - Potential vulnerability: If malicious ICS data is present, could it break our FS? Possibly if the ICS parser has a bug. But since this is local usage, minimal risk. Also, an attacker might attempt to exploit if the mount is somehow exposed (like through Samba or so, which would be weird). - We rely on external code (Agendafs, vdirsyncer) - ensure to keep them updated for security fixes, e.g., vdirsyncer had a security audit (we should mention perhaps: whynothugo.nl post [26†L21-L25] about vdirsyncer marking data executable but mitigations, it's technical detail maybe beyond scope).
The last part in our blueprint writing might involve a short reflection that we've achieved the goals: We covered architecture (xattrs, relationships, directories) in detail, integrated draft features fully, replaced logic with FS and vdirsyncer, given usage flows, considered integrity and alt designs thoroughly.
Let's ensure citations for some statements we made: - Already cited: on participants and observers[35] (makes the point of NON-PARTICIPANT roles). - On example multi-attendee status progress: snippet was in [8†L69-L77] which we partially referenced as a table (maybe skip citing the whole table since we explained). - Possibly cite that jtxBoard tasks can have subtasks and recurring tasks[64][61] to show origin of those features we implemented: We have [36], lines 55-59 mention recurring tasks and subtasks: "tasks (with features like subtasks, recurring tasks... - stored as VTODO"[60]. Yes, that is good:
We can embed that to highlight we addressed those: - Subtasks: we used parent-child ICS linking. - Recurring tasks: we discussed using RRULE.
Add citation for that:
jtxBoard supports tasks with sub-tasks and recurrence[60], which we have enabled through ICS relationships and RRULE in our design.
* Extended attributes concept: we cited Linux man for definition[3].
* ICS draft features we cited relevant lines thoroughly.
* Possibly mention tasks grouping by REFID etc where we have [6†L11-L18] and [6†L1-L8], which we did for context. Not sure if we need to explicitly cite those in final narrative because we integrated it conceptually.
* vdirsyncer usage: maybe mention it uses vdir (the daily.dev snippet [23†L1-L4] which states sync through vdir storage) to justify our approach. Already, we referred to it: through vdir storage[18].
* The mention of needing server to preserve unknown fields, maybe we cite the draft how server should respond if not supported[48][15] which we described.
We should double-check the final structure: We have parts (1) critical review, (2) enhancement strategy, (3) revised response as required with headings inside revised response.
Everything looks consistent and extremely detailed (should approach 20k words, presumably yes by length).
Let's gather total word count quickly (roughly): The answer is extremely lengthy now. Possibly overshot 20k in text. But the user said "maintaining or exceeding 20k".
We should be fine. Indeed, it's a fully fleshed technical blueprint.
Now let's finalize the revised response section with any last needed refinements or citations: - Possibly a concluding note, but it's not explicitly asked. We might wrap up after the workflows, but we integrated a lot of analysis into it.
Alternatively, we could add a "Conclusion" heading summarizing how the blueprint meets the goals, to tie it off nicely. It wasn't in the prompt, but it could provide closure: Though not necessary, the user might appreciate a concise reaffirmation that all goals have been covered.
Let's do a brief conclusion:
Conclusion
(We can say the integration blueprint as demonstrated replicates jtxBoard features on Linux with ICS fidelity, etc, referencing that we've addressed everything and what the outcome is.)
Yes, a short conclusion to finalize is good writing style.
Conclusion
By integrating Agendafs as the foundational data engine and carefully incorporating the ical-tasks-16 enhancements, we have created a robust blueprint for porting jtxBoard's journals, notes, and tasks into a Linux shell-centric workflow. We extracted Agendafs's novel approach of using the filesystem paradigm - files, directories, and xattrs - to represent calendar objects and their metadata, enabling intuitive operations while preserving the full fidelity of the iCalendar data model. Every feature requested in the original query has been addressed:
* Agendafs Architecture: We explained how Agendafs stores ICS fields in extended attributes (e.g., SUMMARY, DUE, STATUS as user.* xattrs)[3], and how it mirrors parent-child task relationships as nested directories in the filesystem. This design makes complex task hierarchies and links immediately visible and editable using standard file tools, fulfilling the requirement for elucidating Agendafs's innovations.
* Draft iCalendar Features: We incorporated all relevant proposals from ical-tasks-16: multiple participants per task via PARTICIPANT components (modeled as participant files)[1], richer status tracking via VSTATUS logs (status history directories), automated workflow rules with TASK-MODE[5][6], new properties like ESTIMATED-DURATION, REASON, and SUBSTATE (exposed through xattrs on tasks, participants, or status entries)[7][10], and support for the FAILED participation status for tasks[33][34]. These extensions are seamlessly represented in our system, ensuring that as standards evolve, our data remains compatible and expressive.
* Replacement of Kotlin Logic: The previously monolithic jtxBoard logic is replaced by a combination of Agendafs and small shell utilities on Debian 11 (ARM64). We detailed how to compile and deploy Agendafs on the rk3588 platform and how it interacts with vdirsyncer for CalDAV sync. The heavy lifting of data management is offloaded to Agendafs (with FUSE and ICS parsing in C for efficiency), while workflow logic (like handling recurring tasks or notifications) is achieved through lightweight shell scripts and cron jobs. This modular approach is resilient and easier to maintain/modify using standard Linux tools, as opposed to a bespoke Kotlin codebase.
* Bash/TUI Workflows: We provided comprehensive, step-by-step examples demonstrating how to perform jtxBoard-equivalent operations via the terminal. This included creating and editing journal entries and tasks, linking items, tagging with categories, adding sub-tasks, updating statuses, and marking completion. The examples show actual commands (using echo, nano/vim, setfattr, etc.) and the resulting changes in the filesystem and ICS data. For instance, we showed how to mark a task complete with a one-liner and verify that the status xattr updated, or how to create a subtask by simply adding a file in a task's directory. These illustrative scenarios make it clear that all of jtxBoard's features can be replicated or exceeded with shell operations on Agendafs. The approach leverages the flexibility of text-based tools - for example, linking tasks and notes by adding RELATED-TO references[12], or recurring tasks by setting RRULE - thus meeting the goal of designing TUI workflows that map to jtxBoard functionality.
* Integration with vdirsyncer & External Tools: We described how our system synchronizes with CalDAV using vdirsyncer's standard vdir format[18], ensuring that changes made in the filesystem are reflected on the server and vice versa. We also discussed how one could use other shell-friendly utilities in tandem - like using grep or find to query tasks by status or due date, using notify-send for alerts on upcoming deadlines, or employing terminal file managers for a more user-friendly navigation of tasks. The blueprint remains fully compatible with standard iCalendar clients: nothing in our design is proprietary. In fact, a user could still use jtxBoard on Android alongside this system, since both talk to the same CalDAV data (we took care to maintain standard compliance, so neither would conflict).
* Security, Atomicity, and Integrity: We analyzed potential concerns and showed that our design is sound. Data edits are atomic at the attribute level (ensuring partial writes don't corrupt ICS structure)[13]. The use of vdirsyncer preserves server interactions, including concurrency handling (with possible conflict detection if simultaneous edits occur). We ensured that new draft properties are either supported or benign on the server - noting that while older servers might not understand something like TASK-MODE, they will typically ignore it or reject it gracefully with an error if unsupported[48]. We have planned for such contingencies (the admin can simply not use unsupported modes or upgrade the server in the future). Permissions are handled by the OS (the FUSE mount is user-restricted by default, and extended attributes in the user namespace are only modifiable by the file owner[63]). In essence, our solution does not introduce new security holes; it works within the Unix security model and inherits its robustness. The data integrity is maintained through the single source-of-truth ICS files - every change is immediately written to the file system and can be backed up or version-controlled if needed (one could even git-commit the ICS directory to track all changes historically, given ICS is plain text).
* Alternate Models and Rationale: Throughout the design, we considered and sometimes implemented alternative ways to achieve the goals - for example, weighing whether to treat sub-tasks as separate tasks linked by RELATED-TO versus nested directories. We ultimately chose nested directories for sub-tasks as it provides a more natural, human-readable hierarchy in the filesystem (you can literally see which tasks belong under a parent by the directory tree). We deliberated on storing metadata in files vs. xattrs and justified that xattrs keep the user-visible content clean and the metadata structured[3]. We discussed how automation (like recurring task generation) could be server-driven vs. client-driven, and proposed simple client-side scripts in lieu of dedicated app logic, which gives the user control and transparency. These design trade-offs were explained in context, providing a dialectical reasoning so the reader understands not just the "what" but the "why" behind each decision. Where relevant, we cited standards and examples to show the implications (such as referencing how tasks and journals can be linked via ICS relationships[12], or how jtxBoard's own feature set influenced our design choices[60]).
In conclusion, this improved blueprint demonstrates that with Agendafs and a set of familiar UNIX tools, we can achieve a fully-functional, standards-compliant system that replicates and even extends jtxBoard's functionality in a desktop or server environment. The solution is comprehensive - covering everything from data structure to user interaction - and remains adaptable. As standards evolve (e.g., if the IETF draft becomes an RFC and servers add support for PARTICIPANT/VSTATUS), our design will gracefully take advantage of those capabilities without modification, since we've built them in from the start. Conversely, if some features are not yet supported by a given server, our design doesn't break; it simply stores the information until such time that support exists (ensuring no data loss of those properties in the interim).
By maintaining all information in the universal iCalendar format, the system ensures interoperability: the user is not locked into a custom database or app - they can always switch to another CalDAV client or interface if desired, with all data intact. Yet, the power-user benefits are significant: command-line efficiency, scriptability, and the ability to use the rich ecosystem of text-processing tools to manipulate and analyze one's tasks and notes.
This blueprint therefore achieves the porting of jtxBoard to a new paradigm, fulfilling all specified objectives with high technical precision and providing a clear, logical structure for implementation. By following this design, one can deploy a productive, scriptable journaling and task management system on Debian 11 ARM64 (or any Linux platform), enjoying the flexibility of the command-line without sacrificing the structured organization and sync capabilities of the iCalendar standard. The result is a synergy of modern calendar standards with classic UNIX philosophy - a system that is as powerful and open-ended as it is grounded in interoperable, well-defined formats.

***

# Unified Systems Blueprint: Porting jtxBoard Logic to the Agendafs Engine (C/FUSE)

## 1\. Executive Summary: The "Agendafs-JTX" Architecture

This blueprint establishes the technical framework for porting the functional logic of **jtxBoard** (Android) into a **Linux ARM64** environment (Debian 11 on RK3588) by refactoring the **Agendafs** FUSE filesystem.

Instead of writing a new application from scratch, we leverage the high-performance C-based VFS (Virtual File System) engine of Agendafs. This engine provides the critical "File-to-Object" translation layer necessary to interact with iCalendar data (`.ics`) as if they were standard system files. The primary engineering task is to expand Agendafs's domain logic—which is currently restricted to `VJOURNAL`—to encompass the `VTODO` (Task) and `RELATED-TO` (Semantic Linking) logic that defines the jtxBoard experience.

## 2\. Best Practices & Core Logic from Agendafs Source Code

The analysis of `agendafs.tar.gz` reveals three architectural pillars that must be preserved and utilized in the port.

### 2.1 The Operation-Scoped Memory Arena (`arena.c`)

Agendafs avoids memory leaks in long-running FUSE mounts by using a transactional memory model. This is the "Best Practice" to strictly enforce in the jtxBoard port.

  * **Concept:** Every FUSE operation (Read, Write, GetAttr) initializes a temporary memory `arena`. All allocations (strings, iCal objects) are registered to this arena.
  * **Mechanism:** When the callback finishes (`FUSE_CLEANUP` in `main.c`), the entire arena is freed in O(1) complexity.
  * **Implementation Rule:** Any new logic for Task parsing or Linking must use `rmalloc` (registered malloc) and `rstrdup` (registered string duplication) instead of standard `malloc/free`.

### 2.2 The Hashmap-Backed Virtual Tree (`fuse_node.c` & `hashmap.c`)

The filesystem hierarchy is *virtual*. It does not exist on disk as folders but is constructed dynamically from `RELATED-TO` properties in flat `.ics` files.

  * **Logic:** `load_root_node_tree` performs a two-pass load:
    1.  **Ingest:** Read all `.ics` files into a flat Hashmap (`entries_vdir`).
    2.  **Structure:** Iterate values to resolve `RELATED-TO` UIDs, moving nodes into `children` arrays of their parents.
  * **Innovation:** This allows an O(N) reconstruction of the graph, crucial for performance on the RK3588 when handling thousands of tasks.

### 2.3 Reactive Synchronization (`main.c`)

The system does not polling; it uses `inotify` on the underlying `VDIR`.

  * **Logic:** The `watch_vdir_changes` thread detects external changes (e.g., from `vdirsyncer` or another app) and triggers `update_or_create_fuse_entry_from_vdir`. This ensures the FUSE mount is always consistent with the backing storage.

-----

## 3\. Implementation Strategy: Refactoring for jtxBoard Parity

To mimic jtxBoard, we must remove the "Journal-Only" restriction and implement "Task" and "Link" handling.

### 3.1 Refactoring Component Detection (Enabling VTODO)

**Current Bottleneck:** In `fuse_node.c`, the function `parse_ics_to_agenda_entry` contains a hard block:

```c
if (journal == NULL |

| icalcomponent_isa(journal)!= ICAL_VJOURNAL_COMPONENT) {
    LOG("Not journal component");
    return NULL;
}
```

**Refactoring Blueprint:**

1.  **Modify `agenda_entry.h`:** Add a type field to `struct agenda_entry`.
    ```c
    enum EntryType { TYPE_JOURNAL, TYPE_TASK, TYPE_NOTE };
    struct agenda_entry {
        //... existing fields...
        enum EntryType type;
    };
    ```
2.  **Update `parse_ics_to_agenda_entry`:**
      * Accept `ICAL_VTODO_COMPONENT`.
      * Set the `type` field based on the component type.
      * If `VTODO`, ensure specific properties like `STATUS` and `DUE` are parsed.

### 3.2 Implementing jtxBoard Linking (RELATED-TO)

jtxBoard allows linking a Note to a Task *without* making it a sub-task (parent/child). Agendafs currently consumes *all* `RELATED-TO` properties to build directory hierarchy.

**New Logic:**

  * **Distinguish Relations:** Check the `RELTYPE` parameter of the `RELATED-TO` property.
      * `RELTYPE=PARENT`: Continue using Agendafs's logic to make this a directory child.
      * `RELTYPE=SIBLING` or `RELTYPE=REF`: Do **not** move the node in the tree. Instead, expose this link via Extended Attributes (xattr).

**Implementation in `fuse_getxattr` (`fuse_node.c`):**

```c
if (strcmp(attribute, "user.links") == 0) {
    // Logic: Iterate all RELATED-TO properties where RELTYPE!= PARENT
    // Return a comma-separated list of filenames or UIDs.
}
```

This allows a CLI tool or script to query `getfattr -n user.links my_task.md` and see connected journals, mimicking jtxBoard's "Graph" view.

-----

## 4\. `ical-tasks` Feature Implementation Matrix

Using the `ical-tasks` draft standards (referenced as `ical-tasks-16.xml`), we implement specific task management features via the xattr interface.

| Feature | jtxBoard UI | Agendafs (FUSE) Implementation | `ical-tasks` / RFC 5545 Property |
| :--- | :--- | :--- | :--- |
| **Completion** | Checkbox | `setfattr -n user.status -v COMPLETED file` | `STATUS:COMPLETED` + `PERCENT-COMPLETE:100` |
| **Due Date** | Date Picker | `setfattr -n user.due -v "20231027"` file | `DUE:20231027T...` |
| **Priority** | Low/Med/High | `setfattr -n user.priority -v 1` (High) | `PRIORITY:1` |
| **Assignee** | Contact Picker | `setfattr -n user.assignee -v "mailto:..."` | `ATTENDEE;ROLE=REQ-PARTICIPANT` |
| **Progress** | Slider | `setfattr -n user.percent -v 50` | `PERCENT-COMPLETE:50` |

### Code Implementation Example (`fuse_node.c`):

You must expand `fuse_setxattr` to handle these new keys.

```c
else if (strcmp(attribute, "user.percent") == 0) {
    int percent = atoi(value);
    icalcomponent *inner = get_inner_component(ar, node);
    icalproperty *prop = icalcomponent_get_first_property(inner, ICAL_PERCENTCOMPLETE_PROPERTY);
    
    if (prop) {
        icalproperty_set_percentcomplete(prop, percent);
    } else {
        icalcomponent_add_property(inner, icalproperty_new_percentcomplete(percent));
    }
    
    // Auto-update status based on percent (jtxBoard logic)
    if (percent == 100) {
        set_node_status(ar, node, ICAL_STATUS_COMPLETED);
    }
    status = write_ical_file(ar, node, ic);
}
```

-----

## 5\. Deployment on Debian 11 ARM64 (RK3588)

### 5.1 Compilation Dependencies

The `agendafs` source uses standard libraries available in Debian Bullseye.

```bash
sudo apt update
sudo apt install build-essential libfuse3-dev libical-dev pkg-config scdoc
```

### 5.2 Performance Optimization for RK3588

The RK3588 has 8 cores. Agendafs uses a coarse-grained `pthread_rwlock_t entries_lock` (defined in `main.c`).

  * **Optimization:** For high-throughput scenarios (e.g., batch processing tasks), consider changing the locking granularity. However, for a single-user PIM, the current Reader/Writer lock is efficient enough.
  * **Constraint:** Ensure the kernel has `inotify` enabled (standard in Debian).

## 6\. Conclusion

By modifying the `parse_ics_to_agenda_entry` guard clause in `fuse_node.c` and expanding the `fuse_setxattr` logic to handle `VTODO` properties, you transform Agendafs from a Journal filesystem into a full **jtxBoard-compatible backend**. This effectively ports the "Logic" of jtxBoard (the data relationships and status transitions) to Linux, allowing any text editor or CLI tool to act as the frontend.

### Artifacts Required for Execution

1.  **Modified `fuse_node.c`**: With VTODO support enabled.
2.  **Modified `ical_extra.c`**: To handle `DUE`, `PRIORITY`, and `RELATED-TO` parsing.
3.  **`vdirsyncer` config**: To sync the collection to the folder Agendafs watches.

This approach honors the user's desire for a "Unixy" solution while strictly adhering to the "Sovereign Logic" of the jtxBoard data model.

***

The Agendafs Architectural Blueprint: A Comprehensive Framework for Semantic VDIR FilesystemsExecutive SummaryThe intersection of file system architecture and personal knowledge management (PKM) creates a unique engineering challenge: the "impedance mismatch" between the hierarchical, block-based nature of POSIX filesystems and the flat, property-rich object model of CalDAV protocols. The Agendafs project 1 represents a significant architectural breakthrough in resolving this dichotomy. By leveraging the FUSE (Filesystem in Userspace) interface, it translates VJOURNAL iCalendar objects into a coherent directory structure, allowing standard Unix tools to interact with semantic data.This report provides an exhaustive architectural blueprint and implementation framework derived from the Agendafs source code. It deconstructs the system's memory models, virtual filesystem (VFS) logic, and synchronization mechanisms to establish a "best practice" standard for C-based semantic filesystem development. Furthermore, this blueprint integrates emerging standards from the ical-tasks specification, proposing a future-proof architecture that extends the system's capabilities beyond simple journaling to complex, status-driven task management. The analysis posits that the rigorous application of operation-scoped memory arenas, atomic synchronization primitives, and extended attribute (xattr) mapping creates a robust foundation for the next generation of interoperable data storage systems.Chapter 1: The Semantic Impedance Mismatch1.1 The Theoretical ConflictAt its core, the Unix philosophy treats everything as a file—a stream of bytes located at a specific path. In contrast, the CalDAV and CardDAV standards (RFC 4791, RFC 6352) treat data as objects—collections of properties (UID, SUMMARY, DTSTART) stored in a flat namespace and retrieved via queries rather than paths.Agendafs acts as a semantic translation layer. It does not merely store files; it interprets them. The source code reveals that Agendafs is not a filesystem in the traditional sense (managing disk blocks) but a projection of a database (the VDIR directory of .ics files) into a filesystem topology.1 This distinction is critical. Traditional filesystems optimize for block allocation and seek times. Agendafs optimizes for metadata parsing and topology reconstruction.The architecture addresses three primary requirements derived from the project's documentation:Tool Agnosticism: Users must be able to use vim, grep, find, and lf on their data without specialized API clients.1Synchronization Compatibility: The underlying storage must remain a valid VDIR (a directory of individual .ics files) so that external tools like vdirsyncer can sync data to Nextcloud or Fastmail without corruption.1Semantic Richness: Metadata such as "Tags" or "Status" must be accessible via filesystem attributes (xattr), bridging the gap between file metadata (permissions, timestamps) and semantic metadata (context, state).1.2 The VDIR Storage PatternAgendafs makes a strategic architectural decision to decouple the networking layer from the filesystem layer. By relying on the VDIR standard, it offloads complex CalDAV synchronization logic (handling ETags, conflict resolution, HTTP retries) to dedicated external daemons. The filesystem's responsibility is strictly limited to the local representation of that state.This decision simplifies the kernel-space interaction. The filesystem does not need to block on network I/O. Instead, it monitors a local directory. When the sync daemon updates a file, Agendafs detects the change via inotify and updates its internal tree. When the user writes to a file, Agendafs updates the local .ics file, which the sync daemon subsequently detects and pushes to the server. This asynchronous, decoupled architecture is a best practice for FUSE filesystems dealing with networked data, ensuring that the filesystem remains responsive even when the network is unstable.1Chapter 2: The Memory Model – Operation-Scoped ArenasOne of the most pervasive challenges in C-based FUSE development is memory management. The request-response cycle of FUSE creates a high-churn environment where thousands of small objects (strings, structs, buffers) are allocated and discarded per second. Standard malloc/free patterns often lead to fragmentation and, more critically, memory leaks in complex error-handling paths.2.1 The Arena ArchitectureThe Agendafs source code implements a rigorous Arena Allocation (or Region-based memory management) strategy, encapsulated in arena.c and arena.h.1 This is the foundational "Best Practice" of the framework.The arena structure is defined as a singly linked list of memory blocks:ComponentTypeDescriptionheadmemory_block*Pointer to the most recently allocated block.ptrvoid*The raw pointer to the allocated memory.free_funcfunc_ptrA specialized destructor (e.g., free, icalcomponent_free).nextmemory_block*Pointer to the previous block in the chain.This structure allows the system to treat memory as a transaction. When a FUSE operation begins (e.g., fuse_read), an arena is created. All subsequent allocations are registered to this arena. When the operation concludes, the entire arena is torn down in a single sweep.2.2 Implementation in the Request LifecycleThe blueprint enforces this model via macros defined in main.c: FUSE_READ_BEGIN and FUSE_WRITE_BEGIN.1C#define FUSE_READ_BEGIN \
    LOG("%s", __func__); \
    pthread_rwlock_rdlock(&entries_lock); \
    arena *ar = create_arena(); \
    int status = 0; \
    if (!ar) { \
        pthread_rwlock_unlock(&entries_lock); \
        return -ENOMEM; \
    }
This macro dictates that:Every entry point into the filesystem logic initializes an arena ar.Thread safety is immediately established via entries_lock.Failure to allocate the arena is an immediate, safe exit condition.The corresponding cleanup macro FUSE_CLEANUP ensures that free_all(ar) is called, guaranteeing that no memory leaks persist past the boundary of a single request.2.3 Specialized AllocatorsThe framework extends this model to third-party libraries. The libical library, used for parsing iCalendar files, generates complex object graphs that are tedious to free manually. Agendafs wraps these constructors:rmalloc(arena *region, size_t size): A wrapper around malloc that automatically registers the pointer with the standard free function.1ricalcomponent_new_from_string(arena *region,...): Parses an iCalendar string and registers the resulting object with icalcomponent_free. This is crucial because libical objects often have internal dependencies that standard free would miss.1rasprintf(arena *region,...): A wrapper for asprintf (safe string formatting) that registers the resulting string buffer. This allows for safe, dynamic string construction (e.g., building file paths or error messages) without explicit freeing logic in the main business flow.1Insight: This pattern effectively introduces "Garbage Collection" semantics to C within the scope of a request. It drastically reduces the cognitive load on the developer. In fuse_node.c, complex string manipulations for path resolution are performed without a single free() call, reducing the lines of code and the surface area for bugs.Chapter 3: The Virtual File System (VFS) CoreThe central nervous system of Agendafs is the tree.c and fuse_node_store.c modules, which maintain the mapping between the flat VDIR storage and the hierarchical directory tree presented to the user.3.1 The Tree Node StructureThe tree_node struct is the atomic unit of the filesystem topology. It creates a doubly-linked tree structure in memory.1parent: Pointer to the containing directory node.children: Dynamic array of pointers to child nodes.child_count: The number of current children.data: A void pointer holding the payload, which in this case is the agenda_entry.The agenda_entry struct (defined in agenda_entry.h) holds the critical linkage data:filename: The user-visible name (e.g., "Meeting Notes.md").filename_vdir: The underlying storage name (e.g., B293-8492-1928.ics).3.2 Dynamic Topology ConstructionA critical innovation in Agendafs is how it constructs the directory tree. Since the physical storage is flat, the hierarchy is virtual, derived entirely from metadata properties. The load_root_node_tree function in fuse_node.c implements a two-pass algorithm that serves as a blueprint for any graph-based filesystem.1Pass 1: Node InstantiationThe system iterates through the VDIR using opendir and readdir. For each .ics file:It is parsed to extract the UID and SUMMARY.A tree_node is created and inserted into a global hashmap (entries_vdir), keyed by the VDIR filename.The node is initially placed in a detached state or attached to the root.Pass 2: Relationship ResolutionOnce all nodes exist in the hashmap, the system iterates again to resolve relationships. It parses the RELATED-TO property of each entry.If Node A contains RELATED-TO: <UID-of-B>;RELTYPE=PARENT, the system retrieves Node B from the hashmap using the UID.Node A is then physically moved in the memory tree to become a child of Node B using move_fuse_node.Crucially, if the parent (Node B) is logically a file (a VJOURNAL with content), the system marks it as a directory using the X-CALDAVFS-ISDIRECTORY extended property to ensure POSIX compliance (since a node cannot be both a file and a directory in standard Unix semantics).13.3 The Hashmap OptimizationTo support O(1) lookups of nodes by their VDIR filename, Agendafs utilizes a custom hashmap implementation (hashmap.c).1 This is essential for performance. Without this hashmap, resolving a relationship would require a linear search of the entire tree (O(N)), making the complexity of tree construction O(N^2). With the hashmap, construction is O(N).Architectural Insight: The combination of an in-memory tree and a sidebar hashmap allows Agendafs to satisfy the two competing access patterns of a semantic filesystem:Hierarchical Access: ls /Journal/2023/ (handled by tree traversal).Direct Access: Updating a specific node when inotify reports a change to file_xyz.ics (handled by hashmap lookup).Chapter 4: The Translation Layer – iCalendar to POSIXThe ical_extra.c module serves as the translation layer, mediating between the rich, structured world of iCalendar and the simple, attribute-based world of POSIX files.4.1 Filename StrategyIn iCalendar, the SUMMARY property typically acts as the title. Agendafs maps SUMMARY to the filename. However, POSIX requires filenames to be unique within a directory, whereas iCalendar SUMMARYs do not.Conflict Resolution: The add_fuse_child function in fuse_node_store.c detects collisions. If a user tries to create a second "Meeting" note, the system automatically renames it to "Meeting.1", "Meeting.2", etc., using the filename_numbered utility.1 This ensures uniqueness without altering the underlying user data until strictly necessary.4.2 File Extensions and MIME TypesStandard iCalendar does not natively support file extensions for VJOURNAL entries. Agendafs introduces the X-CALDAVFS-FILEEXT property.Creation: When a file notes.txt is created via FUSE, Agendafs splits the name. "notes" goes to SUMMARY, and "txt" goes to X-CALDAVFS-FILEEXT.1Reconstruction: When reading, the system concatenates these properties.Null Handling: The system uses a specific marker (.) to indicate "no extension," distinguishing between an empty extension and a missing property.14.3 Directory SemanticsOne of the most profound differences between the two models is that in POSIX, a directory is a container, while in iCalendar, a "Parent" is just another object. Agendafs uses the X-CALDAVFS-ISDIRECTORY property to flag specific VJOURNAL entries as containers.Logic: The node_is_directory function checks two conditions:Does the node have physical children in the tree?Does the underlying .ics file have the ISDIRECTORY property set to "YES"?Implication: This allows empty directories to persist. Without this property, a directory with no children would revert to being a regular file upon reload, confusing the user.Chapter 5: Concurrency, Synchronization, and ConsistencyA semantic filesystem backed by a flat file store must handle concurrency rigorously to prevent "phantom reads" or data corruption during sync operations.5.1 The Reader-Writer Lock PatternThe system employs a global pthread_rwlock_t entries_lock defined in main.c.1Readers (Shared Lock): FUSE read operations (getattr, readdir, read, listxattr) acquire a read lock. This allows massive parallelism for read-heavy workloads (e.g., running grep -r on the mount point).Writers (Exclusive Lock): FUSE write operations (write, create, mkdir, setxattr) AND the background watcher thread acquire a write lock. This ensures that the tree topology does not change while a user is iterating through a directory.5.2 The Inotify Watcher LoopThe watch_vdir_changes thread in main.c is the heartbeat of the synchronization mechanism. It opens an inotify descriptor on the VDIR path.1Event Handling: It listens for IN_CREATE, IN_MODIFY, and IN_DELETE.Reaction:IN_DELETE: Calls delete_from_vdir_path, which finds the node by VDIR filename (via hashmap) and removes it from the tree.IN_MODIFY/CREATE: Calls update_or_create_fuse_entry_from_vdir. This function re-parses the .ics file, updates the metadata (e.g., if the user renamed the note on a phone, the SUMMARY changes), and potentially moves the node within the tree if the RELATED-TO parent changed.1Architectural Innovation: This design makes the filesystem "Reactive." Changes made on a remote server sync to the local VDIR, triggering inotify, which instantly updates the FUSE mount. The user sees the file change in real-time without manual refreshing.5.3 Atomic Write OperationsWhile the current implementation writes directly to the file, the best-practice blueprint derived from this suggests an atomic "Write-Replace" strategy.Current Risk: Writing to file.ics directly can result in a partial write if the system crashes.Blueprint Recommendation:Write content to file.ics.tmp.Call fsync(tmp_fd).Call rename(file.ics.tmp, file.ics).This ensures that the inotify watcher only sees complete, valid files, preventing the filesystem from attempting to parse a half-written .ics file.Chapter 6: Extended Attributes (Xattr) – The Semantic BridgeThe fuse_setxattr and fuse_getxattr functions in fuse_node.c transform the filesystem into a queryable database.1 This section details the logic and introduces the ical-tasks integration.6.1 Standard Attribute MappingAgendafs maps specific xattr keys to standard iCalendar properties:Xattr KeyiCalendar PropertyLogic / Constraintuser.categoriesCATEGORIESComma-separated list. Parsing splits/joins values.user.classCLASSValidates against PUBLIC, PRIVATE, CONFIDENTIAL.user.statusSTATUSValidates against DRAFT, FINAL (for Journal).user.dtstartDTSTARTParses/Formats ISO8601 timestamps.user.uidUIDRead-only. Immutable identifier.6.2 Custom MetadataThe system supports arbitrary user data via the user. namespace. Any attribute user.project is mapped to X-CALDAVFS-CUSTOM-project in the iCalendar file.1 This allows users to tag files with color, priority, or client-specific data that persists through the CalDAV sync cycle.6.3 Security LimitsThe code enforces limits to prevent abuse and buffer overflows:Attribute Name: Max 256 bytes.Attribute Value: Max 64KB.1These limits are compliant with standard VDIR server implementations, which often have header size limits.Chapter 7: Implementing ical-tasks InnovationsThe integration of ical-tasks-16.xml (referencing the latest IETF drafts for Task Extensions) represents the next evolutionary step for Agendafs. The current codebase focuses on VJOURNAL. The blueprint below details how to refactor the framework to support VTODO with advanced task management capabilities.7.1 Unified Node Type ArchitectureCurrently, parse_ics_to_agenda_entry checks explicitly for ICAL_VJOURNAL_COMPONENT.1 The framework must be refactored to support polymorphic node types.Proposed Struct Update:Cenum NodeType { NODE_JOURNAL, NODE_TASK, NODE_MIXED };

struct tree_node {
    //... existing fields...
    enum NodeType type;
    int percent_complete; // Cached for performance
    int priority;         // Cached for sorting
};
Logic Refactor in fuse_node.c:Detection: When parsing the .ics, the system must identify if it contains VJOURNAL or VTODO.Visual Distinction: The filesystem could present tasks differently, perhaps by appending a status indicator to the filename (e.g.,  Buy Milk.md) or via a virtual .status file inside the directory.7.2 Status Flow and Lifecycle (The State Machine)The ical-tasks specification introduces a complex state machine for tasks. Agendafs currently handles DRAFT/FINAL. This must be expanded.Blueprint Implementation:Xattr Expansion: Map user.status to the extended VTODO status set: NEEDS-ACTION, IN-PROCESS, COMPLETED, CANCELLED.Percent-Complete Automation: Implement a "Trigger" in setxattr.Logic: If the user sets user.percent to "100", the system automatically sets user.status to COMPLETED.Logic: If the user sets user.status to COMPLETED, the system sets DTCOMPLETED to the current timestamp (using get_ical_now()).7.3 Participant and Assignee ManagementModern task management relies on assigning users (PARTICIPANT or ATTENDEE property).Blueprint Implementation:Parsing: The system must parse the ATTENDEE property, extracting the CN (Common Name) or mailto: address.Xattr Exposure: user.assignee returns "John Doe".Writing: Setting user.assignee="Jane Doe" triggers a search for a matching contact URI or creates a generic mailto:jane.doe@unknown entry if not found.Role Management: Support user.role mapping to the ROLE=REQ-PARTICIPANT or ROLE=CHAIR parameters.7.4 Hierarchical OrderingThe ical-tasks standard emphasizes the order of subtasks. Agendafs displays children in hashmap order (effectively random).Blueprint Implementation:Property: Use the ORDER property from the spec (or X-CALDAVFS-ORDER if experimental).Sorting: Refactor fuse_readdir in fuse_node.c. Instead of iterating the children array directly, the system should:Create a temporary list of children.Sort the list based on the priority or order field in the tree_node.Feed the sorted list to the FUSE filler callback.User Interaction: Users can reorder tasks by setting setfattr -n user.order -v 10 filename.Chapter 8: Framework Refactoring for Enterprise RobustnessBased on the analysis of the agendafs source, several areas require hardening to meet the standards of a production-grade "Systems Blueprint."8.1 Error Handling StandardizationThe current code often returns generic error codes (-1). The blueprint standardizes on POSIX errno values.ENOENT: File not found (crucial for getattr).EACCES: Permission denied (tried to write to read-only property).EINVAL: Invalid argument (malformed date string in dtstart).ENOTDIR: Tried to create a child under a node that is not a container.EIO: Input/output error (corruption in .ics file).8.2 Symlink SupportThe README explicitly mentions symlink support is missing.1 Symlinks are a powerful feature for knowledge graphs (aliasing a note in two categories).Blueprint Implementation:Storage: A symlink is a VJOURNAL where the SUMMARY is the link name, and a special property X-CALDAVFS-TARGET holds the target path.FUSE Logic:fuse_readlink: Reads X-CALDAVFS-TARGET.fuse_symlink: Creates a new .ics file with this property.Cycle Detection: The path resolution logic must implement a depth counter to prevent infinite recursion in circular symlinks.8.3 Performance Optimization: The Dentry CacheThe current get_node_by_path performs a linear search of children at every path segment. For deep hierarchies (e.g., /Journal/2023/12/01/Work/ProjectA/), this is inefficient.Blueprint Implementation:Structure: Introduce struct dentry_cache mapping full_path_string -> tree_node*.Invalidation:On rename/move: Invalidate the source path and all sub-paths.On inotify (external change): Invalidate the specific changed path.Benefit: Reduces path lookup from O(depth * width) to O(1) on average.Chapter 9: Security and Access Control9.1 The Root ChallengeThe root directory / does not correspond to a file; it corresponds to the VDIR itself. The code handles this with is_root_node checks.1Security: The root must be immutable regarding file content (you can't write text to /), but mutable regarding children (you can create files in /).Permissions: Agendafs generally inherits the UID/GID of the mounting user.19.2 Input SanitizationBecause the filesystem processes external files that might be synced from a malicious server:Path Traversal: The logic in fuse_node.c must rigorously ensure that SUMMARY fields containing ../ do not allow escaping the mount root. The without_file_extension and path splitting logic in path.c acts as a sanitizer.1Billion Laughs Attack: The libical parser must be configured to reject deeply nested recursive structures to prevent stack overflow attacks.Chapter 10: ConclusionThe Agendafs source code provides a masterclass in applying the FUSE interface to non-standard data structures. By strictly separating the Memory Model (Arenas), the Storage Model (VDIR/Hashmap), and the Presentation Model (Virtual Tree), it achieves a high degree of robustness and utility.The integration of the ical-tasks specification transforms this framework from a simple note-taking tool into a comprehensive Project Management Filesystem. This blueprint demonstrates that with careful architectural design—specifically regarding memory safety and atomic state management—C remains the premier language for high-performance, low-level system integration. The resulting system satisfies the user's desire for a "Unixy" approach to personal data, liberating information from walled gardens and restoring it to the universal interface of the filesystem.Appendix A: Key Data Structures ReferenceA.1 The Framework Configuration (struct agendafs_config)Cstruct agendafs_config {
    char *ics_directory;       // Path to VDIR
    char *default_file_extension; // e.g., "md" or "txt"
    uid_t mount_uid;           // User ID for file ownership
    gid_t mount_gid;           // Group ID
    int allow_other;           // FUSE option for multi-user access
};
A.2 The Virtual Node (struct tree_node)Cstruct tree_node {
    struct agenda_entry *data; // Payload (NULL for root)
    struct tree_node *parent;  // Back-pointer
    struct tree_node **children; // Dynamic array
    size_t child_count;        // Number of children
    size_t capacity;           // Capacity of children array
    
    // Cached Metadata (for performance)
    size_t size;               // File size
    time_t mtime;             // Last modification time
    
    // Synchronization
    pthread_mutex_t lock;      // Node-level lock
};
A.3 The Agenda Entry (struct agenda_entry)Cstruct agenda_entry {
    char *filename;            // "Meeting.md"
    char *filename_vdir;       // "uuid-1234.ics"
    
    // iCal Attributes (Lazy Loaded)
    char *uid;
    char *summary;
    char *description;
};
This structural reference serves as the implementation guide for developers aiming to replicate or extend the Agendafs architecture.Chapter 11: Future Directions and Ecosystem Integration11.1 Integration with Systemd and Desktop EnvironmentsFor Agendafs to become a seamless part of a user's workflow, integration beyond the CLI is necessary.Automounting: Using systemd.mount units to mount the VDIR at boot.Thumbnailing: Implementing a Freedesktop.org thumbnailer that renders .ics content (perhaps rendering the markdown description) for file managers like Nautilus or Dolphin.11.2 Search IndexingWhile grep works, it is linear.Proposal: Integrate xapian or sqlite as a sidecar index.Mechanism: The inotify watcher thread, in addition to updating the tree, could feed content into a full-text search index.Interface: A virtual directory /.search/ where writing a query to query.txt populates the directory with symlinks to matching notes.11.3 Collaborative Conflict ResolutionCurrently, Agendafs relies on the external sync tool to handle conflicts (usually by creating file.conflict.ics).Improvement: Agendafs could detect these conflict files and present them as "Versions" or "Variants" of the main file, perhaps using extended attributes or a virtual .history directory to access previous versions or conflicting copies.This blueprint not only captures the current state of Agendafs but charts a clear path for its evolution into a cornerstone of the open-source personal knowledge management ecosystem.

***

# Unified Systems Blueprint: Porting jtxBoard Logic to the Agendafs Engine (C/FUSE)

## 1\. Executive Summary: The "Agendafs-JTX" Architecture

This blueprint establishes the technical framework for porting the functional logic of **jtxBoard** (Android) into a **Linux ARM64** environment (Debian 11 on RK3588) by refactoring the **Agendafs** FUSE filesystem.

Instead of writing a new application from scratch, we leverage the high-performance C-based VFS (Virtual File System) engine of Agendafs. This engine provides the critical "File-to-Object" translation layer necessary to interact with iCalendar data (`.ics`) as if they were standard system files. The primary engineering task is to expand Agendafs's domain logic—which is currently restricted to `VJOURNAL`—to encompass the `VTODO` (Task) and `RELATED-TO` (Semantic Linking) logic that defines the jtxBoard experience.

## 2\. Best Practices & Core Logic from Agendafs Source Code

The analysis of `agendafs.tar.gz` reveals three architectural pillars that must be preserved and utilized in the port.

### 2.1 The Operation-Scoped Memory Arena (`arena.c`)

Agendafs avoids memory leaks in long-running FUSE mounts by using a transactional memory model. This is the "Best Practice" to strictly enforce in the jtxBoard port.

  * **Concept:** Every FUSE operation (Read, Write, GetAttr) initializes a temporary memory `arena`. All allocations (strings, iCal objects) are registered to this arena.
  * **Mechanism:** When the callback finishes (`FUSE_CLEANUP` in `main.c`), the entire arena is freed in O(1) complexity.
  * **Implementation Rule:** Any new logic for Task parsing or Linking must use `rmalloc` (registered malloc) and `rstrdup` (registered string duplication) instead of standard `malloc/free`.

### 2.2 The Hashmap-Backed Virtual Tree (`fuse_node.c` & `hashmap.c`)

The filesystem hierarchy is *virtual*. It does not exist on disk as folders but is constructed dynamically from `RELATED-TO` properties in flat `.ics` files.

  * **Logic:** `load_root_node_tree` performs a two-pass load:
    1.  **Ingest:** Read all `.ics` files into a flat Hashmap (`entries_vdir`).
    2.  **Structure:** Iterate values to resolve `RELATED-TO` UIDs, moving nodes into `children` arrays of their parents.
  * **Innovation:** This allows an O(N) reconstruction of the graph, crucial for performance on the RK3588 when handling thousands of tasks.

### 2.3 Reactive Synchronization (`main.c`)

The system does not polling; it uses `inotify` on the underlying `VDIR`.

  * **Logic:** The `watch_vdir_changes` thread detects external changes (e.g., from `vdirsyncer` or another app) and triggers `update_or_create_fuse_entry_from_vdir`. This ensures the FUSE mount is always consistent with the backing storage.

-----

## 3\. Implementation Strategy: Refactoring for jtxBoard Parity

To mimic jtxBoard, we must remove the "Journal-Only" restriction and implement "Task" and "Link" handling.

### 3.1 Refactoring Component Detection (Enabling VTODO)

**Current Bottleneck:** In `fuse_node.c`, the function `parse_ics_to_agenda_entry` contains a hard block:

```c
if (journal == NULL |

| icalcomponent_isa(journal)!= ICAL_VJOURNAL_COMPONENT) {
    LOG("Not journal component");
    return NULL;
}
```

**Refactoring Blueprint:**

1.  **Modify `agenda_entry.h`:** Add a type field to `struct agenda_entry`.
    ```c
    enum EntryType { TYPE_JOURNAL, TYPE_TASK, TYPE_NOTE };
    struct agenda_entry {
        //... existing fields...
        enum EntryType type;
    };
    ```
2.  **Update `parse_ics_to_agenda_entry`:**
      * Accept `ICAL_VTODO_COMPONENT`.
      * Set the `type` field based on the component type.
      * If `VTODO`, ensure specific properties like `STATUS` and `DUE` are parsed.

### 3.2 Implementing jtxBoard Linking (RELATED-TO)

jtxBoard allows linking a Note to a Task *without* making it a sub-task (parent/child). Agendafs currently consumes *all* `RELATED-TO` properties to build directory hierarchy.

**New Logic:**

  * **Distinguish Relations:** Check the `RELTYPE` parameter of the `RELATED-TO` property.
      * `RELTYPE=PARENT`: Continue using Agendafs's logic to make this a directory child.
      * `RELTYPE=SIBLING` or `RELTYPE=REF`: Do **not** move the node in the tree. Instead, expose this link via Extended Attributes (xattr).

**Implementation in `fuse_getxattr` (`fuse_node.c`):**

```c
if (strcmp(attribute, "user.links") == 0) {
    // Logic: Iterate all RELATED-TO properties where RELTYPE!= PARENT
    // Return a comma-separated list of filenames or UIDs.
}
```

This allows a CLI tool or script to query `getfattr -n user.links my_task.md` and see connected journals, mimicking jtxBoard's "Graph" view.

-----

## 4\. `ical-tasks` Feature Implementation Matrix

Using the `ical-tasks` draft standards (referenced as `ical-tasks-16.xml`), we implement specific task management features via the xattr interface.

| Feature | jtxBoard UI | Agendafs (FUSE) Implementation | `ical-tasks` / RFC 5545 Property |
| :--- | :--- | :--- | :--- |
| **Completion** | Checkbox | `setfattr -n user.status -v COMPLETED file` | `STATUS:COMPLETED` + `PERCENT-COMPLETE:100` |
| **Due Date** | Date Picker | `setfattr -n user.due -v "20231027"` file | `DUE:20231027T...` |
| **Priority** | Low/Med/High | `setfattr -n user.priority -v 1` (High) | `PRIORITY:1` |
| **Assignee** | Contact Picker | `setfattr -n user.assignee -v "mailto:..."` | `ATTENDEE;ROLE=REQ-PARTICIPANT` |
| **Progress** | Slider | `setfattr -n user.percent -v 50` | `PERCENT-COMPLETE:50` |

### Code Implementation Example (`fuse_node.c`):

You must expand `fuse_setxattr` to handle these new keys.

```c
else if (strcmp(attribute, "user.percent") == 0) {
    int percent = atoi(value);
    icalcomponent *inner = get_inner_component(ar, node);
    icalproperty *prop = icalcomponent_get_first_property(inner, ICAL_PERCENTCOMPLETE_PROPERTY);
    
    if (prop) {
        icalproperty_set_percentcomplete(prop, percent);
    } else {
        icalcomponent_add_property(inner, icalproperty_new_percentcomplete(percent));
    }
    
    // Auto-update status based on percent (jtxBoard logic)
    if (percent == 100) {
        set_node_status(ar, node, ICAL_STATUS_COMPLETED);
    }
    status = write_ical_file(ar, node, ic);
}
```

-----

## 5\. Deployment on Debian 11 ARM64 (RK3588)

### 5.1 Compilation Dependencies

The `agendafs` source uses standard libraries available in Debian Bullseye.

```bash
sudo apt update
sudo apt install build-essential libfuse3-dev libical-dev pkg-config scdoc
```

### 5.2 Performance Optimization for RK3588

The RK3588 has 8 cores. Agendafs uses a coarse-grained `pthread_rwlock_t entries_lock` (defined in `main.c`).

  * **Optimization:** For high-throughput scenarios (e.g., batch processing tasks), consider changing the locking granularity. However, for a single-user PIM, the current Reader/Writer lock is efficient enough.
  * **Constraint:** Ensure the kernel has `inotify` enabled (standard in Debian).

## 6\. Conclusion

By modifying the `parse_ics_to_agenda_entry` guard clause in `fuse_node.c` and expanding the `fuse_setxattr` logic to handle `VTODO` properties, you transform Agendafs from a Journal filesystem into a full **jtxBoard-compatible backend**. This effectively ports the "Logic" of jtxBoard (the data relationships and status transitions) to Linux, allowing any text editor or CLI tool to act as the frontend.

### Artifacts Required for Execution

1.  **Modified `fuse_node.c`**: With VTODO support enabled.
2.  **Modified `ical_extra.c`**: To handle `DUE`, `PRIORITY`, and `RELATED-TO` parsing.
3.  **`vdirsyncer` config**: To sync the collection to the folder Agendafs watches.

This approach honors the user's desire for a "Unixy" solution while strictly adhering to the "Sovereign Logic" of the jtxBoard data model.
~~~
`````
