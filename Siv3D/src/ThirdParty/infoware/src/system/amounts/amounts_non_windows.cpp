// infoware - C++ System information Library
//
// Written in 2016-2020 by nabijaczleweli <nabijaczleweli@gmail.com> and ThePhD <phdofthehouse@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>


#ifndef _WIN32


#include "infoware/detail/scope.hpp"
#include "infoware/system.hpp"
#include <cstring>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
// [Siv3D] iOS には wordexp が無い (数えるのは Linux の /dev/input なので、iOS では常に 0)
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define SIV3D_INFOWARE_NO_WORDEXP 1
#else
#include <wordexp.h>
#endif

template <class Callback>
class ScopeGuard final
{
private:

	Callback m_callback;

public:

	ScopeGuard() = delete;

	ScopeGuard(const ScopeGuard&) = delete;

	ScopeGuard(Callback&& callback)
		: m_callback(callback)
	{

	}

	~ScopeGuard()
	{
		m_callback();
	}
};

// http://man7.org/linux/man-pages/man3/wordexp.3.html
static std::size_t count_expansions(const char* of) noexcept {
#ifdef SIV3D_INFOWARE_NO_WORDEXP
	(void)of;
	return 0;
#else
	wordexp_t exp{};
	ScopeGuard exp_deleter{[&]() { wordfree(&exp); }};

	if(wordexp(of, &exp, 0))
		return 0;

	// If nothing is expanded wordexp() returns the original string
	if(exp.we_wordc == 1 && std::strcmp(exp.we_wordv[0], of) == 0)
		return 0;
	else
		return exp.we_wordc;
#endif
}


// http://unix.stackexchange.com/questions/25601/how-do-mouse-events-work-in-linux
// https://www.kernel.org/doc/Documentation/input/input.txt section 3.2.2
std::size_t iware::system::mouse_amount() noexcept {
	return count_expansions("/dev/input/mouse*");
}

// https://www.kernel.org/doc/Documentation/input/input.txt section 3.2.1
// Doesn't specify that and there doesn't seem to be a way to get it
std::size_t iware::system::keyboard_amount() noexcept {
	return 0;
}

std::size_t iware::system::other_HID_amount() noexcept {
	return 0;
}


#endif
