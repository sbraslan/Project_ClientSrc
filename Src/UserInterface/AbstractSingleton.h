#pragma once

#include <cstdint>

template <typename T> class TAbstractSingleton
{
	static T * ms_singleton;

public:
	TAbstractSingleton()
	{
		assert(!ms_singleton);

		const std::intptr_t offset =
			reinterpret_cast<std::intptr_t>((T*)1) -
			reinterpret_cast<std::intptr_t>((CSingleton<T>*)(T*)1);

		ms_singleton = reinterpret_cast<T*>(
			reinterpret_cast<std::intptr_t>(this) + offset
		);
	}

	TAbstractSingleton(const TAbstractSingleton &) = delete;
	TAbstractSingleton(TAbstractSingleton &&) = delete;
	TAbstractSingleton & operator=(const TAbstractSingleton &) = delete;
	TAbstractSingleton & operator=(TAbstractSingleton &&) = delete;

	virtual ~TAbstractSingleton()
	{
		assert(ms_singleton);
		ms_singleton = nullptr;
	}

	__forceinline static T & GetSingleton()
	{
		assert(ms_singleton != nullptr);
		return (*ms_singleton);
	}
};

template <typename T> T * TAbstractSingleton<T>::ms_singleton = nullptr;
