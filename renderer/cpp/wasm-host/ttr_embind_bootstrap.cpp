/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

QTBUG-131279 workaround. Qt for WebAssembly resolves the system locale
through `navigatorLanguages()`, which calls
`emscripten::vecFromJSArray<std::string>` and therefore needs embind's
`std::string` binding. That binding lives in libembind's own static
constructor, which is linked last and does not run before Qt's first
locale query, so the lookup fails with "emval::as has unknown type".

This translation unit registers the binding itself, from a constructor
that runs before the renderer touches Qt. It must be compiled with
-fno-rtti so `TypeID<std::string>` resolves to the same LightTypeID that
Qt for WebAssembly (also built without RTTI) asks for; the CMakeLists
enforces that.
*/
#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>

#include <limits>
#include <string>

namespace Ttr {

// QTBUG-131279. Qt for WebAssembly resolves the system locale through
// navigatorLanguages(), which converts a JS array via embind and therefore
// needs embind's primitive and std::string bindings. Those live in
// libembind's own static constructor, which as a system library is linked
// last - after Qt's first locale query - so the conversion fails with
// "emval::as has unknown type".
//
// Registering them here, before any Qt object is constructed, fixes the
// ordering. This TU must keep RTTI enabled: Qt for WebAssembly is built
// with RTTI, so its embind type ids are &typeid(T); a -fno-rtti build
// would register under LightTypeID ids that Qt never asks for.
void RegisterEmbindPrimitives() {
	static const auto once = [] {
		using namespace emscripten::internal;
		// Touching the type ids is what matters: taking TypeID<T>::get()
		// from this translation unit pulls libembind's registration
		// objects into the link ahead of Qt, so their constructors run
		// before Qt's first locale query. The values are deliberately
		// discarded - registering here instead would collide with
		// libembind's own registrations ("Cannot register type twice").
		const auto touch = [](const void *id) { asm volatile("" :: "r"(id)); };
		touch(TypeID<std::string>::get());
		touch(TypeID<unsigned long>::get());
		touch(TypeID<unsigned int>::get());
		touch(TypeID<int>::get());
		touch(TypeID<double>::get());
		touch(TypeID<char>::get());
		touch(TypeID<bool>::get());
		touch(TypeID<emscripten::val>::get());
		return 0;
	}();
	(void)once;
}

} // namespace Ttr

#endif // __EMSCRIPTEN__
