#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <string.h>
#include <map>
#include "VDeleter.h"

///Structure that stores the index of a Queue Family
struct QueueFamilyIndices
{
	int graphicsFamily = -1;

	bool isComplete()
	{
		return graphicsFamily >= 0;
	}
};

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
#pragma region App Variables
	GLFWwindow* window;																							//Reference to the GLFW Window
	const int WIDTH = 800;																						//Window width
	const int HEIGHT = 600;																						//Window height
	VDeleter<VkInstance> instance{ vkDestroyInstance };															//Vulkan Instance, wrapped in VDeleter object
	VDeleter<VkDebugReportCallbackEXT> callback{ instance, DestroyDebugReportCallbackEXT };						//Callback Handle
	VDeleter<VkSurfaceKHR> surface{ instance, vkDestroySurfaceKHR };											//Surface Handle
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;															//Device (GPU) Handle
	VDeleter<VkDevice> device{ vkDestroyDevice };																//Logical Device Handle
	VkQueue graphicsQueue;																						//Queue Handle

	const std::vector<const char*> validationLayers =															//Validation Layers
	{
		"VK_LAYER_LUNARG_standard_validation"
	};
#pragma endregion

#ifdef NDEBUG																									//Enables validations layers if in Debug mode
	const bool enableValidationLayers = false;					
#else
	const bool enableValidationLayers = true;
#endif

	///<summary>Checks for supported Validation Layers</summary>
	bool checkValidationLayerSupport()
	{
		//Checks for the amount of available validation layers
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		//Gets the available validation layers
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());


		//Check if the layers in validationLayers are in the available layers
		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	///<summary>Gets required GLFW Extensions, as well as required extensions for validation layers, if debug enabled</summar>
	std::vector<const char*> getRequiredExtensions()
	{
		std::vector<const char*> extensions;

		//Gets all the extensions required by glfw
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		//Adds them to the return vector
		for (unsigned int i = 0; i < glfwExtensionCount; i++)
		{
			extensions.push_back(glfwExtensions[i]);
		}

		//Adds the validation layer extension, if debug is enabled
		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}


	///<summary>Debug Callback Function</summary>
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
														VkDebugReportObjectTypeEXT objType,
														uint64_t obj,
														size_t location,
														int32_t code,
														const char* layerPrefix,
														const char* msg,
														void* userData)
	{

		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	///<summary>Setups up the Callback handle</summary>
	void setupDebugCallback()
	{
		//Early exit if not in debug mode
		if (!enableValidationLayers) return;

		//Setup Callback info struct
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		//Creates and stores the callback handle
		if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, callback.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug callback!");
		}
	}

	///<summary>Select an available physical device and setup device handle</summary>
	void pickPhysicalDevice()
	{
		//Gets count of physical devices the support Vulkan
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		//Throw error if there are none
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		//Gets list of physical devices
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); 

		//Finds the suitability of each device
		std::map<int, VkPhysicalDevice> candidates;
		for (const auto& device : devices)
		{
			int score = rateDeviceSuitability(device);
			candidates[score] = device;
		}

		// Check if the best candidate is suitable at all
		if (candidates.begin()->first > 0)
		{
			physicalDevice = candidates.begin()->second;
		}
		else
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	///<summary>Finds the Queue Families supported by the Vulkan Physical Device</summary>
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDeviceCandidate)
	{
		//Gets the count of queue family properties
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceCandidate, &queueFamilyCount, nullptr);

		//Gets a list of queue family properties
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceCandidate, &queueFamilyCount, queueFamilies.data());
		
		//Finds a queue family that supports VK_QUEUE_GRAPHICS_BIT
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}


		return indices;
	}

	///<summary>Returns an int score for the provided Vulkan Physical Device based on its graphics capabilities</summary>
	int rateDeviceSuitability(VkPhysicalDevice physicalDeviceCandidate)
	{
		//Gets the properties and features of the device candidate
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDeviceCandidate, &deviceProperties);
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(physicalDeviceCandidate, &deviceFeatures);

		//Makes sure the device meets minimum requirements
		if (!isDeviceSuitable(physicalDeviceCandidate))
			return 0;

		int score = 1;

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		return score;
	}

	///<summary>Determines if a Vulkan Physical Device is meets the minimum requirements for this application</summary>
	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		//Gets properties and features of candidate
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		//Checks to make sure that the device supports geometry shaders
		if (!deviceFeatures.geometryShader)
			return false;

		//Makes sure the device has queue families
		QueueFamilyIndices indices = findQueueFamilies(device);

		return indices.isComplete();
	}

	///<summary>Initializes the GLFW window</summary>
	void initWindow() 
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	///<summary>Creates and stores a Logical Device handle for our selected physical device</summary>
	void createLogicalDevice()
	{
		//Gets the queue families of the physical device
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		//Creates a queue info struct
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
		queueCreateInfo.queueCount = 1;

		//Sets the queue priority
		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		
		//Creates the device features struct
		VkPhysicalDeviceFeatures deviceFeatures = {};

		//Creates the logical device infro struct
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 0;

		//Sets up the validation layers if debug is enabled
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		//Creates and stores the logical device handle
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, device.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		//Stores device queue handle
		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
	}

	///<summary></summary>
	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, surface.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}

	}

	///<summary>Initializes Vulkan</summary>
	void initVulkan()
	{
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
	}

	///<summary>Creates Vulkan Instance</summary>
	void createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layers requested, but not available!");
		}

		//Creates application info struct
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		//Creates instance info struct
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;


		//Adds extension info to instance struct
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		//Enable Global Validation Layers in instance struct
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		//Actual instance creation
		if (vkCreateInstance(&createInfo, nullptr, instance.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
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

