#pragma once
#include "pch.h"
#include "EasyCEFHooks.h"

#define CAST_TO(target,to) reinterpret_cast<decltype(&to)>(target)

_cef_frame_t* EasyCEFHooks::frame = NULL;
cef_v8context_t* EasyCEFHooks::contextl = NULL;

struct _cef_client_t* EasyCEFHooks::cef_client = NULL;
PVOID EasyCEFHooks::origin_cef_browser_host_create_browser=NULL;
PVOID EasyCEFHooks::origin_cef_get_keyboard_handler = NULL;
PVOID EasyCEFHooks::origin_cef_on_key_event = NULL;
PVOID EasyCEFHooks::origin_cef_v8context_get_current_context = NULL;
PVOID EasyCEFHooks::origin_cef_load_handler = NULL;
PVOID EasyCEFHooks::origin_cef_on_load_start = NULL;
std::function<void(struct _cef_browser_t* browser,struct _cef_frame_t* frame,cef_transition_type_t transition_type)> EasyCEFHooks::onLoadStart = [](auto browser, auto frame, auto transition_type) {};
std::function<void(_cef_client_t*, struct _cef_browser_t*, const struct _cef_key_event_t*)> EasyCEFHooks::onKeyEvent=[](auto client,auto browser,auto key){};

void EasyCEFHooks::executeJavaScript(_cef_frame_t* frame,string script, string url) {
	CefString exec_script = script;
	CefString purl = url;
	frame->execute_java_script(frame, exec_script.GetStruct(), purl.GetStruct(), 0);
}

cef_v8context_t* EasyCEFHooks::hook_cef_v8context_get_current_context() {
	cef_v8context_t* context = CAST_TO(origin_cef_v8context_get_current_context, hook_cef_v8context_get_current_context)();

	cef_browser_t* browser = context->get_browser(context);
	auto host = browser->get_host(browser);


	contextl = context;
	frame = browser->get_main_frame(browser);

	return context;
}

int CEF_CALLBACK EasyCEFHooks::hook_cef_on_key_event(struct _cef_keyboard_handler_t* self,
	struct _cef_browser_t* browser,
	const struct _cef_key_event_t* event,
	cef_event_handle_t os_event) {
	onKeyEvent(cef_client, browser, event);

	return CAST_TO(origin_cef_on_key_event, hook_cef_on_key_event)(self, browser, event, os_event);
}


struct _cef_keyboard_handler_t* CEF_CALLBACK EasyCEFHooks::hook_cef_get_keyboard_handler(struct _cef_client_t* self) {
	auto keyboard_handler = CAST_TO(origin_cef_get_keyboard_handler, hook_cef_get_keyboard_handler)(self);
	if (keyboard_handler) {
		cef_client = self;
		origin_cef_on_key_event = keyboard_handler->on_key_event;
		keyboard_handler->on_key_event = hook_cef_on_key_event;
	}
	return keyboard_handler;
}



struct _cef_load_handler_t* CEF_CALLBACK EasyCEFHooks::hook_cef_load_handler(struct _cef_client_t* self) {
	auto load_handler = CAST_TO(origin_cef_load_handler, hook_cef_load_handler)(self);
	if (load_handler) {
		cef_client = self;
		origin_cef_on_load_start = load_handler->on_load_start;
		load_handler->on_load_start = hook_cef_on_load_start;
	}
	return load_handler;
}

void CEF_CALLBACK EasyCEFHooks::hook_cef_on_load_start(struct _cef_load_handler_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	cef_transition_type_t transition_type) {
	CAST_TO(origin_cef_on_load_start, hook_cef_on_load_start)(self, browser, frame, transition_type);
	onLoadStart(browser, frame, transition_type);
}

int EasyCEFHooks::hook_cef_browser_host_create_browser(
	const cef_window_info_t* windowInfo,
	struct _cef_client_t* client,
	const cef_string_t* url,
	const struct _cef_browser_settings_t* settings,
	struct _cef_request_context_t* request_context) {

	origin_cef_get_keyboard_handler = client->get_keyboard_handler;
	client->get_keyboard_handler = hook_cef_get_keyboard_handler;

	origin_cef_load_handler = client->get_load_handler;
	client->get_load_handler = hook_cef_load_handler;

	int origin = CAST_TO(origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser)
		(windowInfo, client, url, settings, request_context);
	return origin;
}

bool EasyCEFHooks::InstallHooks() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	origin_cef_v8context_get_current_context = DetourFindFunction("libcef.dll", "cef_v8context_get_current_context");
	origin_cef_browser_host_create_browser = DetourFindFunction("libcef.dll", "cef_browser_host_create_browser_sync");
	if (origin_cef_v8context_get_current_context) {
		DetourAttach(&origin_cef_v8context_get_current_context, (PVOID)hook_cef_v8context_get_current_context);
	}
	else {
		return false;
	}
	if (origin_cef_browser_host_create_browser) {
		DetourAttach(&origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser);
	}
	else {
		return false;
	}
	LONG ret = DetourTransactionCommit();
	return ret == NO_ERROR;
}

bool EasyCEFHooks::UninstallHook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&origin_cef_v8context_get_current_context, hook_cef_v8context_get_current_context);
	DetourDetach(&origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser);
	LONG ret = DetourTransactionCommit();
	return ret == NO_ERROR;
}