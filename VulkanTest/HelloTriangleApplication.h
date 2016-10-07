#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <string.h>
#include "VDeleter.h"


///Main class for basic triangle drawing
class HelloTriangleApplication 
{
public:
	///<summary>Calls the functions for window/Vulkan initialization, as well as starts the main loop</summary>
	void run() 
	{
		initWindow();
		initVulkan();
		mainLoop();
	}

private:
	GLFWwindow* window;											//Reference to the GLFW Window
	const int WIDTH = 800;										//Window width
	const int HEIGHT = 600;										//Window height
	VDeleter<VkInstance> instance{ vkDestroyInstance };			//Vulkan Instance, wrapped in VDeleter object

	///<summary>Initializes the GLFW window</summary>
	void initWindow() 
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	///<summary>Initializes Vulkan</summary>
	void initVulkan() 
	{
		createInstance();
	}

	///<summary>Creates Vulkan Instance</summary>
	void createInstance()
	{
		//Creates application info struct
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		//Creates instance infor struct
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;


		//Gets GLFW Instance Extension
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);


		//Adds extension info to instance struct
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		//Global Validation Layer Setting
		createInfo.enabledLayerCount = 0;

		//Actual instance creation
		if (vkCreateInstance(&createInfo, nullptr, instance.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
		}

		//Query for supported extensions
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		//Checks if GLFW's required extensions are supported, throws runtime error if not
		std::cout << "required extensions:" << std::endl;
		for (unsigned int i = 0; i < glfwExtensionCount; ++i)
		{
			bool found = false;
			for (const auto& extension : extensions)
			{
				if (strcmp(extension.extensionName,glfwExtensions[i]) == 0)
					found = true;
			}
			if (found)
				std::cout << "\t" << glfwExtensions[i] << " is supported" << std::endl;
			else
				throw std::runtime_error("Missing Required Extension");
		}
	}

	///<summary>Main Application Loop</summary>
	void mainLoop() 
	{
		while (!glfwWindowShouldClose(window)) 
		{
			glfwPollEvents();
		}
	}
};

