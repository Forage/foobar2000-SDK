#pragma once

//! API for tag read/write operations. Legal to call from main thread only, except for hint_multi_async() / hint_async() / hint_reader().\n
//! Implemented only by core, do not reimplement.\n
//! Use static_api_ptr_t template to access metadb_io methods.\n
//! WARNING: Methods that perform file access (tag reads/writes) run a modal message loop. They SHOULD NOT be called from global callbacks and such.
class NOVTABLE metadb_io : public service_base
{
public:
	enum t_load_info_type {
		load_info_default,
		load_info_force,
		load_info_check_if_changed
	};

	enum t_update_info_state {
		update_info_success,
		update_info_aborted,
		update_info_errors,		
	};
	
	enum t_load_info_state {
		load_info_success,
		load_info_aborted,
		load_info_errors,		
	};

	//! No longer used - returns false always.
	__declspec(deprecated) virtual bool is_busy() = 0;
	//! No longer used - returns false always.
	__declspec(deprecated) virtual bool is_updating_disabled() = 0;
	//! No longer used - returns false always.
	__declspec(deprecated) virtual bool is_file_updating_blocked() = 0;
	//! No longer used.
	__declspec(deprecated) virtual void highlight_running_process() = 0;
	//! Loads tags from multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_load_info_state load_info_multi(metadb_handle_list_cref p_list,t_load_info_type p_type,HWND p_parent_window,bool p_show_errors) = 0;
	//! Updates tags on multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state update_info_multi(metadb_handle_list_cref p_list,const pfc::list_base_const_t<file_info*> & p_new_info,HWND p_parent_window,bool p_show_errors) = 0;
	//! Rewrites tags on multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state rewrite_info_multi(metadb_handle_list_cref p_list,HWND p_parent_window,bool p_show_errors) = 0;
	//! Removes tags from multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state remove_info_multi(metadb_handle_list_cref p_list,HWND p_parent_window,bool p_show_errors) = 0;

	virtual void hint_multi(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_infos,const pfc::list_base_const_t<t_filestats> & p_stats,const bit_array & p_fresh_mask) = 0;

	virtual void hint_multi_async(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_infos,const pfc::list_base_const_t<t_filestats> & p_stats,const bit_array & p_fresh_mask) = 0;

	virtual void hint_reader(service_ptr_t<class input_info_reader> p_reader,const char * p_path,abort_callback & p_abort) = 0;

	//! For internal use only.
	virtual void path_to_handles_simple(const char * p_path, metadb_handle_list_ref p_out) = 0;

	//! Dispatches metadb_io_callback calls with specified items. To be used with metadb_display_field_provider when your component needs specified items refreshed.
	virtual void dispatch_refresh(metadb_handle_list_cref p_list) = 0;

	void dispatch_refresh(metadb_handle_ptr const & handle) {dispatch_refresh(pfc::list_single_ref_t<metadb_handle_ptr>(handle));}

	void hint_async(metadb_handle_ptr p_item,const file_info & p_info,const t_filestats & p_stats,bool p_fresh);

	__declspec(deprecated) t_load_info_state load_info(metadb_handle_ptr p_item,t_load_info_type p_type,HWND p_parent_window,bool p_show_errors);
	__declspec(deprecated) t_update_info_state update_info(metadb_handle_ptr p_item,file_info & p_info,HWND p_parent_window,bool p_show_errors);
	
	FB2K_MAKE_SERVICE_COREAPI(metadb_io);
};

//! Implementing this class gives you direct control over which part of file_info gets altered during a tag update uperation. To be used with metadb_io_v2::update_info_async().
class NOVTABLE file_info_filter : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(file_info_filter, service_base);
public:
	//! Alters specified file_info entry; called as a part of tag update process. Specified file_info has been read from a file, and will be written back.\n
	//! WARNING: This will be typically called from another thread than main app thread (precisely, from thread created by tag updater). You should copy all relevant data to members of your file_info_filter instance in constructor and reference only member data in apply_filter() implementation.
	//! @returns True when you have altered file_info and changes need to be written back to the file; false if no changes have been made.
	virtual bool apply_filter(metadb_handle_ptr p_location,t_filestats p_stats,file_info & p_info) = 0;
};

//! Extended file_info_filter allowing the caller to do their own manipulation of the file before and after the metadata update takes place. \n
//! Respected by foobar2000 v1.5 and up; if metadb_io_v4 is supported, then file_info_filter_v2 is understood.
class NOVTABLE file_info_filter_v2 : public file_info_filter {
	FB2K_MAKE_SERVICE_INTERFACE(file_info_filter_v2, file_info_filter);
public:

	enum filterStatus_t {
		filterNoUpdate = 0,
		filterProceed,
		filterAlreadyUpdated
	};
	//! Called after just before rewriting metadata. The file is not yet opened for writing, but a file_lock has already been granted (so don't call it on your own). \n
	//! You can use this method to perform album art updates (via album_art_editor API) alongside metadata updates. \n
	//! Return value can be used to stop fb2k from proceeding with metadata update on this file. \n
	//! If your own operations on this file fail, just pass the exceptions to the caller and they will be reported just as other tag update errors.
	//! @param fileIfAlreadyOpened Reference to an already opened file object, if already opened by the caller. May be null.
	virtual filterStatus_t before_tag_update( const char * location, file::ptr fileIfAlreadyOpened, abort_callback & aborter ) = 0;

	//! Called after metadata has been updated. \n
	//! If you wish to alter the file on your own, use before_tag_update() for this instead. \n
	//! If your own operations on this file fail, just pass the exceptions to the caller and they will be reported just as other tag update errors. \n
	//! The passed reader object can be used to read the properties of the updated file back. In most cases it will be the writer that was used to update the tags. Do not call tag writing methods on it from this function.
	virtual void after_tag_update( const char * location, service_ptr_t<class input_info_reader> reader, abort_callback & aborter ) = 0;

	virtual void after_all_tag_updates( abort_callback & aborter ) = 0;

	//! Allows you to do your own error logging.
	//! @returns True if the error has been noted by your code and does not need to be shown to the user.
	virtual bool filter_error( const char * location, const char * msg ) = 0;
};

//! Advanced interface for passing infos read from files to metadb backend. Use metadb_io_v2::create_hint_list() to instantiate. \n
//! Thread safety: all methods other than on_done() are intended for worker threads. Instantiate and use the object in a worker thread, call on_done() in main thread to finalize. \n
//! Typical usage pattern: create a hint list (in any thread), hand infos to it from files that you work with (in a worker thread), call on_done() in main thread. \n
class NOVTABLE metadb_hint_list : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(metadb_hint_list,service_base);
public:
	//! Helper.
	static metadb_hint_list::ptr create();
	//! Adds a hint to the list.
	//! @param p_location Location of the item the hint applies to.
	//! @param p_info file_info object describing the item.
	//! @param p_stats Information about the file containing item the hint applies to.
	//! @param p_freshflag Set to true if the info has been directly read from the file, false if it comes from another source such as a playlist file.
	virtual void add_hint(metadb_handle_ptr const & p_location,const file_info & p_info,const t_filestats & p_stats,bool p_freshflag) = 0;
	//! Reads info from specified info reader instance and adds hints. May throw an exception in case info read has failed. \n
	//! If the file has multiple subsongs, info from all the subsongs will be read and pssed to add_hint(). \n
	//! Note that an input_info_writer is a subclass of input_info_reader - so any input_info_reader OR input_info_writer is a valid argument for add_hint_reader(). \n
	//! This method is often called with your input_info_writer instance after committing tag updates, to notify metadb about altered tags.
	virtual void add_hint_reader(const char * p_path,service_ptr_t<input_info_reader> const & p_reader,abort_callback & p_abort) = 0;
	//! Call this when you're done working with this metadb_hint_list instance, to apply hints and dispatch callbacks. \n
	//! If you don't call this, all added hints will be ignored. \n
	//! As a general rule, you should add as many infos as possible - such as all the tracks involved in some operation that you perform - then call on_done() once. \n
	//! on_done() is expensive because it not only updates the metadb, but tells all components about the changes made - refreshes playlists/autoplaylists, library viewers, etc. \n
	//! Calling on_done() repeatedly is inefficient and should be avoided.
	virtual void on_done() = 0;
};

//! \since 1.0
//! To obtain metadb_hint_list_v2, use service_query on a metadb_hint_list object. \n
//! Simplified: metadb_hint_list_v2::ptr v2; v2 ^= myHintList;  ( causes bugcheck if old fb2k / no interface ).
class NOVTABLE metadb_hint_list_v2 : public metadb_hint_list {
	FB2K_MAKE_SERVICE_INTERFACE(metadb_hint_list_v2, metadb_hint_list);
public:
	//! Helper.
	static metadb_hint_list_v2::ptr create();
	//! Hint with browse info. \n
	//! See: metadb_handle::get_browse_info() for browse info rationale.
	//! @param p_location Location for which we're providing browse info.
	//! @param p_info Browse info for this location.
	//! @param browseTS timestamp of the browse info - such as last-modified time of the playlist file providing browse info.
	virtual void add_hint_browse(metadb_handle_ptr const & p_location,const file_info & p_info, t_filetimestamp browseTS) = 0;
};

//! \since 1.3
//! To obtain metadb_hint_list_v3, use service_query on a metadb_hint_list object. \n
//! Simplified: metadb_hint_list_v3::ptr v3; v3 ^= myHintList;  ( causes bugcheck if old fb2k / no interface ).
class NOVTABLE metadb_hint_list_v3 : public metadb_hint_list_v2 {
	FB2K_MAKE_SERVICE_INTERFACE(metadb_hint_list_v3, metadb_hint_list_v2);
public:
	//! Helper.
	static metadb_hint_list_v3::ptr create();
	//! Hint primary info with a metadb_info_container.
	virtual void add_hint_v3(metadb_handle_ptr const & p_location, metadb_info_container::ptr info,bool p_freshflag) = 0;
	//! Hint browse info with a metadb_info_container.
	virtual void add_hint_browse_v3(metadb_handle_ptr const & p_location,metadb_info_container::ptr info) = 0;

	//! Add a forced hint.\n
	//! A normal hint may or may not cause metadb update - metadb is not updated if the file has not changed according to last modified time. \n
	//! A forced hint always updates metadb regardless of timestamps.
	virtual void add_hint_forced(metadb_handle_ptr const & p_location, const file_info & p_info,const t_filestats & p_stats,bool p_freshflag) = 0;
	//! Add a forced hint, with metadb_info_container. \n
	//! Forced hint rationale - see add_hint_forced().
	virtual void add_hint_forced_v3(metadb_handle_ptr const & p_location, metadb_info_container::ptr info,bool p_freshflag) = 0;
	//! Adds a forced hint, with an input_info_reader. \n
	//! Forced hint rationale - see add_hint_forced(). \n
	//! Info reader use rationale - see add_hint_reader(). 
	virtual void add_hint_forced_reader(const char * p_path,service_ptr_t<input_info_reader> const & p_reader,abort_callback & p_abort) = 0;
};


//! New in 0.9.3. Extends metadb_io functionality with nonblocking versions of tag read/write functions, and some other utility features.
class NOVTABLE metadb_io_v2 : public metadb_io {
public:
	enum {
		//! By default, when some part of requested operation could not be performed for reasons other than user abort, a popup dialog with description of the problem is spawned.\n
		//! Set this flag to disable error notification.
		op_flag_no_errors		= 1 << 0,
		//! Set this flag to make the progress dialog not steal focus on creation.
		op_flag_background		= 1 << 1,
		//! Set this flag to delay the progress dialog becoming visible, so it does not appear at all during short operations. Also implies op_flag_background effect.
		op_flag_delay_ui		= 1 << 2,

		//! \since 1.3
		//! Indicates that the caller is aware of the metadb partial info feature introduced at v1.3.
		//! When not specified, affected info will be quietly preserved when updating tags.
		op_flag_partial_info_aware = 1 << 3,
	};

	//! Preloads information from the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_load_info_state values. Can be null if caller doesn't care.
	virtual void load_info_async(metadb_handle_list_cref p_list,t_load_info_type p_type,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Updates tags of the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	//! @param p_filter Callback handling actual file_info alterations. Typically used to replace entire meta part of file_info, or to alter something else such as ReplayGain while leaving meta intact.
	virtual void update_info_async(metadb_handle_list_cref p_list,service_ptr_t<file_info_filter> p_filter,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Rewrites tags of the specified tracks; similar to update_info_async() but using last known/cached file_info values rather than values passed by caller.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	virtual void rewrite_info_async(metadb_handle_list_cref p_list,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Strips all tags / metadata fields from the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	virtual void remove_info_async(metadb_handle_list_cref p_list,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;

	//! Creates a metadb_hint_list object. \n
	//! Contrary to other metadb_io methods, this can be safely called in a worker thread. You only need to call the hint list's on_done() method in main thread to finalize.
	virtual metadb_hint_list::ptr create_hint_list() = 0;

	//! Updates tags of the specified tracks. Helper; uses update_info_async internally.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	//! @param p_new_info New infos to write to specified items.
	void update_info_async_simple(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_new_info, HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify);

	//! Helper to be called after a file has been rechaptered. \n
	//! Forcibly reloads info then tells playlist_manager to update all affected playlists.
	void on_file_rechaptered( const char * path, metadb_handle_list_cref newItems );
	//! Helper to be called after a file has been rechaptered. \n
	//! Forcibly reloads info then tells playlist_manager to update all affected playlists.
	void on_files_rechaptered( metadb_handle_list_cref newHandles );

	FB2K_MAKE_SERVICE_COREAPI_EXTENSION(metadb_io_v2,metadb_io);
};


//! Dynamically-registered version of metadb_io_callback. See metadb_io_callback for documentation, register instances using metadb_io_v3::register_callback(). It's recommended that you use the metadb_io_callback_dynamic_impl_base helper class to manage registration/unregistration.
class NOVTABLE metadb_io_callback_dynamic {
public:
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;
};

//! \since 0.9.5
class NOVTABLE metadb_io_v3 : public metadb_io_v2 {
public:
	virtual void register_callback(metadb_io_callback_dynamic * p_callback) = 0;
	virtual void unregister_callback(metadb_io_callback_dynamic * p_callback) = 0;

	FB2K_MAKE_SERVICE_COREAPI_EXTENSION(metadb_io_v3,metadb_io_v2);
};


class threaded_process_callback;

#if FOOBAR2000_TARGET_VERSION >= 80
//! \since 1.5
class NOVTABLE metadb_io_v4 : public metadb_io_v3 {
	FB2K_MAKE_SERVICE_COREAPI_EXTENSION(metadb_io_v4, metadb_io_v3);
public:
	//! Creates an update-info task, that can be either fed to threaded_process API, or invoked by yourself respecting threaded_process semantics. \n
	//! May return null pointer if the operation has been refused (by user settings or such). \n
	//! Useful for performing the operation with your own in-dialog progress display instead of the generic progress popup.
	virtual service_ptr_t<threaded_process_callback> spawn_update_info( metadb_handle_list_cref items, service_ptr_t<file_info_filter> p_filter, uint32_t opFlags, completion_notify_ptr reply ) = 0;
	//! Creates an remove-info task, that can be either fed to threaded_process API, or invoked by yourself respecting threaded_process semantics. \n
	//! May return null pointer if the operation has been refused (by user settings or such). \n
	//! Useful for performing the operation with your own in-dialog progress display instead of the generic progress popup.
	virtual service_ptr_t<threaded_process_callback> spawn_remove_info( metadb_handle_list_cref items, uint32_t opFlags, completion_notify_ptr reply) = 0;
	//! Creates an load-info task, that can be either fed to threaded_process API, or invoked by yourself respecting threaded_process semantics. \n
	//! May return null pointer if the operation has been refused (for an example no loading is needed for these items). \n
	//! Useful for performing the operation with your own in-dialog progress display instead of the generic progress popup.
	virtual service_ptr_t<threaded_process_callback> spawn_load_info( metadb_handle_list_cref items, t_load_info_type opType, uint32_t opFlags, completion_notify_ptr reply) = 0;
};
#endif

//! metadb_io_callback_dynamic implementation helper.
class metadb_io_callback_dynamic_impl_base : public metadb_io_callback_dynamic {
public:
	void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) {}
	
	metadb_io_callback_dynamic_impl_base() {static_api_ptr_t<metadb_io_v3>()->register_callback(this);}
	~metadb_io_callback_dynamic_impl_base() {static_api_ptr_t<metadb_io_v3>()->unregister_callback(this);}

	PFC_CLASS_NOT_COPYABLE_EX(metadb_io_callback_dynamic_impl_base)
};
//! Callback service receiving notifications about metadb contents changes.
class NOVTABLE metadb_io_callback : public service_base {
public:
	//! Called when metadb contents change. (Or, one of display hook component requests display update).
	//! @param p_items_sorted List of items that have been updated. The list is always sorted by pointer value, to allow fast bsearch to test whether specific item has changed.
	//! @param p_fromhook Set to true when actual file contents haven't changed but one of metadb_display_field_provider implementations requested an update so output of metadb_handle::format_title() etc has changed.
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io_callback);
};

//! \since 1.1
//! Callback service receiving notifications about user-triggered tag edits. \n
//! You want to use metadb_io_callback instead most of the time, unless you specifically want to track tag edits for purposes other than updating user interface.
class NOVTABLE metadb_io_edit_callback : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io_edit_callback)
public:
	//! Called after the user has edited tags on a set of files.
	typedef const pfc::list_base_const_t<const file_info*> & t_infosref;
	virtual void on_edited(metadb_handle_list_cref items, t_infosref before, t_infosref after) = 0;
};

//! Entrypoint service for metadb_handle related operations.\n
//! Implemented only by core, do not reimplement.\n
//! Use metadb::get() to obtain an instance.
class NOVTABLE metadb : public service_base
{
protected:	
	//! OBSOLETE, DO NOT CALL
	virtual void database_lock()=0;
	//! OBSOLETE, DO NOT CALL
	virtual void database_unlock()=0;
public:
	
	//! Returns a metadb_handle object referencing the specified location. If one doesn't exist yet a new one is created. There can be only one metadb_handle object referencing specific location. \n
	//! This function should never fail unless there's something critically wrong (can't allocate memory for the new object, etc). \n
	//! Speed: O(log(n)) to total number of metadb_handles present. It's recommended to pass metadb_handles around whenever possible rather than pass playable_locations then retrieve metadb_handles on demand when needed.
	//! @param p_out Receives the metadb_handle pointer.
	//! @param p_location Location to create a metadb_handle for.
	virtual void handle_create(metadb_handle_ptr & p_out,const playable_location & p_location)=0;

	void handle_create_replace_path_canonical(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path);
	void handle_replace_path_canonical(metadb_handle_ptr & p_out,const char * p_new_path);
	void handle_create_replace_path(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path);

	//! Helper function; attempts to retrieve a handle to any known playable location to be used for e.g. titleformatting script preview.\n
	//! @returns True on success; false on failure (no known playable locations).
	static bool g_get_random_handle(metadb_handle_ptr & p_out);

	enum {case_sensitive = playable_location::case_sensitive};
	typedef playable_location::path_comparator path_comparator;

	inline static int path_compare_ex(const char * p1,t_size len1,const char * p2,t_size len2) {return case_sensitive ? pfc::strcmp_ex(p1,len1,p2,len2) : stricmp_utf8_ex(p1,len1,p2,len2);}
	inline static int path_compare_nc(const char * p1, size_t len1, const char * p2, size_t len2) {return case_sensitive ? pfc::strcmp_nc(p1,len1,p2,len2) : stricmp_utf8_ex(p1,len1,p2,len2);}
	inline static int path_compare(const char * p1,const char * p2) {return case_sensitive ? strcmp(p1,p2) : stricmp_utf8(p1,p2);}
	inline static int path_compare_metadb_handle(const metadb_handle_ptr & p1,const metadb_handle_ptr & p2) {return path_compare(p1->get_path(),p2->get_path());}

	metadb_handle_ptr handle_create(playable_location const & l) {metadb_handle_ptr temp; handle_create(temp, l); return temp;}
	metadb_handle_ptr handle_create(const char * path, uint32_t subsong) {return handle_create(make_playable_location(path, subsong));}

	FB2K_MAKE_SERVICE_COREAPI(metadb);
};

class titleformat_text_out;
class titleformat_hook_function_params;


/*!
	Implementing this service lets you provide your own title-formatting fields that are parsed globally with each call to metadb_handle::format_title methods. \n
	Note that this API is meant to allow you to add your own component-specific fields - not to overlay your data over standard fields or over fields provided by other components. Any attempts to interfere with standard fields will have severe ill effects. \n
	This should be implemented only where absolutely necessary, for safety and performance reasons. Any expensive operations inside the process_field() method may severely damage performance of affected title-formatting calls. \n
	You must NEVER make any other foobar2000 API calls from inside process_field, other than possibly querying information from the passed metadb_handle pointer; you should read your own implementation-specific private data and return as soon as possible. You must not make any assumptions about calling context (threading etc). \n
	It is guaranteed that process_field() is called only inside a metadb lock scope so you can safely call "locked" metadb_handle methods on the metadb_handle pointer you get. You must not lock metadb by yourself inside process_field() - while it is always called from inside a metadb lock scope, it may be called from another thread than the one maintaining the lock because of multi-CPU optimizations active. \n
	If there are multiple metadb_display_field_provider services registered providing fields of the same name, the behavior is undefined. You must pick unique names for provided fields to ensure safe coexistence with other people's components. \n
	IMPORTANT: Any components implementing metadb_display_field_provider MUST call metadb_io::dispatch_refresh() with affected metadb_handles whenever info that they present changes. Otherwise, anything rendering title-formatting strings that reference your data will not update properly, resulting in unreliable/broken output, repaint glitches, etc. \n
	Do not expect a process_field() call each time somebody uses title formatting, calling code might perform its own caching of strings that you return, getting new ones only after metadb_io::dispatch_refresh() with relevant items. \n
	If you can't reliably notify other components about changes of content of fields that you provide (such as when your fields provide some kind of global information and not information specific to item identified by passed metadb_handle), you should not be providing those fields in first place. You must not change returned values of your fields without dispatching appropriate notifications. \n
	Use static service_factory_single_t<myclass> to register your metadb_display_field_provider implementations. Do not call other people's metadb_display_field_provider services directly, they're meant to be called by backend only. \n
	List of fields that you provide is expected to be fixed at run-time. The backend will enumerate your fields only once and refer to them by indexes later. \n
*/

class NOVTABLE metadb_display_field_provider : public service_base {
public:
	//! Returns number of fields provided by this metadb_display_field_provider implementation.
	virtual t_uint32 get_field_count() = 0;
	//! Returns name of specified field provided by this metadb_display_field_provider implementation. Names are not case sensitive. It's strongly recommended that you keep your field names plain English / ASCII only.
	virtual void get_field_name(t_uint32 index, pfc::string_base & out) = 0;
	//! Evaluates the specified field.
	//! @param index Index of field being processed : 0 <= index < get_field_count().
	//! @param handle Handle to item being processed. You can safely call "locked" methods on this handle to retrieve track information and such.
	//! @param out Interface receiving your text output.
	//! @returns Return true to indicate that the field is present so if it's enclosed in square brackets, contents of those brackets should not be skipped, false otherwise.
	virtual bool process_field(t_uint32 index, metadb_handle * handle, titleformat_text_out * out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_display_field_provider);
};





//! Helper implementation of file_info_filter_impl.
class file_info_filter_impl : public file_info_filter {
public:
	file_info_filter_impl(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,const pfc::list_base_const_t<const file_info*> & p_new_info) {
		FB2K_DYNAMIC_ASSERT(p_list.get_count() == p_new_info.get_count());
		pfc::array_t<t_size> order;
		order.set_size(p_list.get_count());
		order_helper::g_fill(order.get_ptr(),order.get_size());
		p_list.sort_get_permutation_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,order.get_ptr());
		m_handles.set_count(order.get_size());
		m_infos.set_size(order.get_size());
		for(t_size n = 0; n < order.get_size(); n++) {
			m_handles[n] = p_list[order[n]];
			m_infos[n] = *p_new_info[order[n]];
		}
	}

	bool apply_filter(metadb_handle_ptr p_location,t_filestats p_stats,file_info & p_info) {
		t_size index;
		if (m_handles.bsearch_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,p_location,index)) {
			p_info = m_infos[index];
			return true;
		} else {
			return false;
		}
	}
private:
	metadb_handle_list m_handles;
	pfc::array_t<file_info_impl> m_infos;
};


//! \since 1.1
//! metadb_index_manager hash, currently a 64bit int, typically made from halving MD5 hash.
typedef t_uint64 metadb_index_hash;


//! \since 1.1
//! A class that transforms track information (location+metadata) to a hash for metadb_index_manager. \n
//! You need to implement your own when using metadb_index_manager to pin your data to user's tracks. \n
//! Possible routes to take when implementing: \n
//! Rely on location only - pinning lost when user moves, but survives editing tags\n
//! Rely on metadata - pinning survives moving files, but lost when editing tags\n
//! If you do the latter, you can implement metadb_io_edit_callback to respond to tag edits and avoid data loss.
class NOVTABLE metadb_index_client : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(metadb_index_client, service_base)
public:
	virtual metadb_index_hash transform(const file_info & info, const playable_location & location) = 0;

	bool hashHandle(metadb_handle_ptr const & h, metadb_index_hash & out) {
		metadb_info_container::ptr info;
		if (!h->get_info_ref(info)) return false;
		out = transform(info->info(), h->get_location());
		return true;
	}

	static metadb_index_hash from_md5(hasher_md5_result const & in) {return in.xorHalve();}
};

//! \since 1.1
//! This service lets you pin your data to user's music library items, typically to be presented as title formatting %fields% via metadb_display_field_provider. \n
//! Implement metadb_index_client to define how your data gets pinned to the songs.
class NOVTABLE metadb_index_manager : public service_base {
	FB2K_MAKE_SERVICE_COREAPI(metadb_index_manager)
public:
	//! Install a metadb_index_client. \n
	//! This is best done from init_stage_callback::on_init_stage(init_stages::before_config_read) to avoid hammering already loaded UI & playlists with refresh requests. \n
	//! If you provide your own title formatting fields, call dispatch_global_refresh() after a successful add() to signal all components to refresh all tracks \n
	//! - which is expensive, hence it should be done in early app init phase for minimal performance penalty. \n
	//! Always put a try/catch around add() as it may fail with an exception in an unlikely scenario of corrupted files holding your previously saved data. \n
	//! @param client your metadb_index_client object.
	//! @param index_id Your GUID that you will pass to other methods when referring to your index.
	//! @param userDataRetentionPeriod Time for which the data should be retained if no matching tracks are present. \n
	//! If this was set to zero, the data could be lost immediately if a library folder disappers for some reason. \n
	//! Hint: use system_time_periods::* constants, for an example, system_time_periods::week.
	virtual void add(metadb_index_client::ptr client, const GUID & index_id, t_filetimestamp userDataRetentionPeriod) = 0;
	//! Uninstalls a previously installed index.
	virtual void remove(const GUID & index_id) = 0;
	//! Sets your data for the specified index+hash.
	virtual void set_user_data(const GUID & index_id, const metadb_index_hash & hash, const void * data, t_size dataSize) = 0;
	//! Gets your data for the specified index+hash.
	virtual void get_user_data(const GUID & index_id, const metadb_index_hash & hash, mem_block_container & out) = 0;


	//! Helper
	template<typename t_array> void get_user_data_t(const GUID & index_id, const metadb_index_hash & hash, t_array & out) {
		mem_block_container_ref_impl<t_array> ref(out);
		get_user_data(index_id, hash, ref);
	}

	//! Helper
	t_size get_user_data_here(const GUID & index_id, const metadb_index_hash & hash, void * out, t_size outSize) {
		mem_block_container_temp_impl ref(out, outSize);
		get_user_data(index_id, hash, ref);
		return ref.get_size();
	}

	//! Signals all components that your data for the tracks matching the specified hashes has been altered; this will redraw the affected tracks in playlists and such.
	virtual void dispatch_refresh(const GUID & index_id, const pfc::list_base_const_t<metadb_index_hash> & hashes) = 0;
	
	//! Helper
	void dispatch_refresh(const GUID & index_id, const metadb_index_hash & hash) {
		pfc::list_single_ref_t<metadb_index_hash> l(hash);
		dispatch_refresh(index_id, l);
	}

	//! Dispatches a global refresh, asks all components to refresh all tracks. To be calling after adding/removing indexes. Expensive!
	virtual void dispatch_global_refresh() = 0;

	//! Efficiently retrieves metadb_handles of items present in the Media Library matching the specified index value. \n
	//! This can be called from the app main thread only (interfaces with the library_manager API).
	virtual void get_ML_handles(const GUID & index_id, const metadb_index_hash & hash, metadb_handle_list_ref out) = 0;

	//! Retrieves all known hash values for this index.
	virtual void get_all_hashes(const GUID & index_id, pfc::list_base_t<metadb_index_hash> & out) = 0;

	//! Determines whether a no longer needed user data file for this index exists. \n
	//! For use with index IDs that are not currently registered only.
	virtual bool have_orphaned_data(const GUID & index_id) = 0;

	//! Deletes no longer needed index user data files. \n
	//! For use with index IDs that are not currently registered only.
	virtual void erase_orphaned_data(const GUID & index_id) = 0;

	//! Saves index user data file now. You normally don't need to call this; it's done automatically when saving foobar2000 configuration. \n
	//! This will throw exceptions in case of a failure (out of disk space etc).
	virtual void save_index_data(const GUID & index_id) = 0;
};
