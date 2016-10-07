#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>

/// VDeleter handles the deletion of a Vulkan Object
template <typename T>
class VDeleter {
public:
	///<summary>Default Constructor, keeps a "dummy" cleanup function, for use when the object will be created later, such as in lists</summary>
	VDeleter() : VDeleter([](T, VkAllocationCallbacks*) {}) {}

	///<summary>Used for Vulkan Objects where only the object itself needs to be provided to the respective cleanup function</summay>
	VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef)
	{
		this->deleter = [=](T obj) { deletef(obj, nullptr); };
	}

	///<summary>Used for Vulkan Objects where a VKInstance also needs to be passed into the cleanup function</summary>
	VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) 
	{
		this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
	}

	///<summary>Used for Vulkan Objects where a VKDevice also needs to be passed into the cleanup function</summary>
	VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) 
	{
		this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
	}

	///<summary>Calls the respective cleanup function when the VDeleter object goes out of scope</summary>
	~VDeleter() 
	{
		cleanup();
	}

	///<summary>Overloaded reference operator returns a constant point to the contained object, preventing it from being unexpectedly changed</summary>
	const T* operator &() const
	{
		return &object;
	}

	///<summary>Cleans up the contained object before returning a reference to it so that it may be overwritten</summary>
	T* replace() 
	{
		cleanup();
		return &object;
	}

	///<summary>Overloaded conversion operator</summary>
	operator T() const 
	{
		return object;
	}

	///<summary>Overloaded assignment operator for object of the same type</summary>
	void operator=(T rhs) 
	{
		if (rhs != object) 
		{
			cleanup();
			object = rhs;
		}
	}

	///<summary>Overloaded assignment operator for object of a different type</summary>
	template<typename V>
	bool operator==(V rhs) 
	{
		return object == T(rhs);
	}

private:
	T object{ VK_NULL_HANDLE };				//Vulkan Object Handler
	std::function<void(T)> deleter;			//Deleter function

	///<summary>Calls the respective cleanup function, as long as there is a currently stored object</summary>
	void cleanup() {
		if (object != VK_NULL_HANDLE) 
		{
			deleter(object);
		}
		object = VK_NULL_HANDLE;
	}
};

