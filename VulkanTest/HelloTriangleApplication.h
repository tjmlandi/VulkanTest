#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <string.h>
#include <map>
#include <set>
#include <algorithm>  
#include <fstream>
#include "VDeleter.h"

///Structure that stores the index of a Queue Family
struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete()
	{
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

///Structure that stores the swap chain information
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
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
	VkQueue graphicsQueue;																						//Graphics Queue Handle
	VkQueue presentQueue;																						//Surface Presentation Queue Handle
	VDeleter<VkSwapchainKHR> swapChain{ device, vkDestroySwapchainKHR };										//Swap Chain Handle
	std::vector<VkImage> swapChainImages;																		//Swap Chain Image Handles
	VkFormat swapChainImageFormat;																				//Swap Chain Image Format
	VkExtent2D swapChainExtent;																					//Swap Chain Extent
	std::vector<VDeleter<VkImageView>> swapChainImageViews;														//Image View Objects
	VDeleter<VkRenderPass> renderPass{ device, vkDestroyRenderPass };											//Render Pass Reference
	VDeleter<VkPipelineLayout> pipelineLayout{ device, vkDestroyPipelineLayout };								//Pipeline layout, used to set shader variables at drawing time
	VDeleter<VkPipeline> graphicsPipeline{ device, vkDestroyPipeline };											//Graphics Pipeline Object
	std::vector<VDeleter<VkFramebuffer>> swapChainFramebuffers;													//Container of all the swap chain image framebuffers
	VDeleter<VkCommandPool> commandPool{ device, vkDestroyCommandPool };										//Vulkan command pool, manages the command buffers and memory for them
	std::vector<VkCommandBuffer> commandBuffers;																//Container for command buffers. Will be automatically freed when pool is destroyed


	const std::vector<const char*> validationLayers =															//Required Validation Layers
	{
		"VK_LAYER_LUNARG_standard_validation"
	};

	const std::vector<const char*> deviceExtensions =															//Required Device Extensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
			//Set the index of the queue family that supports drawing
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			//Find out if there is support for presenting to the surface, and which queue family it is
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDeviceCandidate, i, surface, &presentSupport);

			//Set the index of the queue family that supports presenting to the surface
			if (queueFamily.queueCount > 0 && presentSupport)
			{
				indices.presentFamily = i;
			}

			//Exit if we've found a queue families with the capabilities we want
			if (indices.isComplete())
			{
				break;
			}

			i++;
		}
		return indices;
	}

	///<summary>Finds the supported info of the swap chain</summary>
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		//Get the swap chain capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		//Get the count of surface formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		//Store the surface formats
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		//Get the count of presentation modes
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		//Store the presentation modes
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
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

		QueueFamilyIndices indices = findQueueFamilies(physicalDeviceCandidate);

		//Better performance if presentation and drawing queue families are the same
		if (indices.graphicsFamily == indices.presentFamily) score += 100;

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		return score;
	}

	///<summary>Determines if a Vulkan Physical Device is meets the minimum requirements for this application</summary>
	bool isDeviceSuitable(VkPhysicalDevice physicalDeviceCandidate)
	{
		//Gets properties and features of candidate
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDeviceCandidate, &deviceProperties);
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(physicalDeviceCandidate, &deviceFeatures);

		//Checks to make sure that the device supports geometry shaders
		if (!deviceFeatures.geometryShader)
			return false;
		
		//Makes sure the device has queue families
		QueueFamilyIndices indices = findQueueFamilies(physicalDeviceCandidate);

		//Make sure the device supports the required exensions
		bool extensionsSupported = checkDeviceExtensionSupport(physicalDeviceCandidate);

		//Make sure the swap chain supports our requirements
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDeviceCandidate);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	///<summary>Checks if the device supports the required extensions</summary>
	bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDeviceCandidate)
	{
		//Get count of supported extensions
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDeviceCandidate, nullptr, &extensionCount, nullptr);

		//Get list of supported extensions
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDeviceCandidate, nullptr, &extensionCount, availableExtensions.data());

		//Make a set of the required extensions
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		//Iterate through the required extensions set, and remove ones that are found to be supported
		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		//Return whether or not there are required extensions that were not found to be supported
		return requiredExtensions.empty();
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

		//Creates a Queue Family struct for each unique queue family
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };
		float queuePriority = 1.0f;
		for (int queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//Creates the device features struct
		VkPhysicalDeviceFeatures deviceFeatures = {};

		//Creates the logical device infro struct
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = deviceExtensions.size();
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

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

		//Stores graphics queue handle
		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);

		//Stores presentation queue handle
		vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
	}

	///<summary>Creates and stores a glfw Surface handle</summary>
	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, surface.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}

	}

	///<summary>Create a swap chain with desired specifications, or the closest supported ones</summary>
	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		//Call helper functions to get surface format, presentation mode, and swap extent
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		//Try to include an extra image in the swap chain if possible for triple buffering, otherwise default to the max image count
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		//Create swap chain struct
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		//Get the queue families for the device
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

		//If the presentation and graphics queues are the same, use exclusive sharing mode. Otherwise, for simplicity, use concurrrent mode <CHANGE>
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		//Specifying that there will not be a transformation on the swap chain
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

		//Ignore the alpha channel when compositing windows
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		//Set the presentation mode to the one selected earlier
		createInfo.presentMode = presentMode;

		//Turns on clipping so Vulkan can discard render operations for obscured sections of the window
		createInfo.clipped = VK_TRUE;

		//Assumption that there will only be one swap chain, for now <CHANGE>
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		//Create and store the swap chain handle
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, swapChain.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		//Gets and stores the swap chain image handles
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		//Store the image format and extent
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	///<summary>Chooses the surface format based on the available formats provided</summary>
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		//Return desired format if the swap chain has no preferred formats
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		//If the swap chain does have preferred formats, see if desired format is one of them
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		//All else fails, just use the first available format
		return availableFormats[0];
	}

	///<summary>Chooses the presentation mode based on the available modes provided</summary>
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
	{
		//Look for Mailbox mode if available
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}

		//Otherwise, use FIFO
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	///<summary>Choose the swap extent based on the provided capabilities of the swap chain</summary>
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		//If capable extent is provided, use it
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		//Otherwise, use the resolution of the window, or as close as possible within the capabilities
		else
		{
			VkExtent2D actualExtent = { WIDTH, HEIGHT };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	///<summary>Creates basic image view for every image in the Swap Chain</summary>
	void createImageViews()
	{
		//Resize image views list to accomodate for total image view count
		swapChainImageViews.resize(swapChainImages.size(), VDeleter<VkImageView>{device, vkDestroyImageView});

		for (uint32_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];

			//Specify image interpretation
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;

			//Set default channel swizzle
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			//Set up the images as color targets without mip levels or multiple layers
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			//Create and store the Image View Object Handle
			if (vkCreateImageView(device, &createInfo, nullptr, swapChainImageViews[i].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	///<summary>Helper function to load binary data from shader files</summary>
	static std::vector<char> readFile(const std::string& filename)
	{
		//open binary file, from end
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		//throw error if it failes to open
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		//get file size and create buffer of that size
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		//go to start of file and read into buffer
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		//close file
		file.close();

		return buffer;
	}

	///<summary>Give shader code, creates a shader module objects</summary>
	void createShaderModule(const std::vector<char>& code, VDeleter<VkShaderModule>& shaderModule) 
	{
		//struct to define options for the VkShaderModule
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = (uint32_t*)code.data();

		//create shader module with the provided options
		if (vkCreateShaderModule(device, &createInfo, nullptr, shaderModule.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
	}

	///<summary>Creates a render pass object with one subpass and a color attachment</summary>
	void createRenderPass()
	{
		//Struct defining swap chain color attachment
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;							//Sets format to match the swap chain
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;						//Sets one sample, as there is no multisampling currently
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					//Clears framebuffer to black before drawing new frame
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;					//Stores the rendered contents so we can view them one screen
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;		//Currently not doing anything with the stencil buffer, so leave these as don't care
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				//We don't care about the pixel format of the buffer before rendering
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;			//Set the format of the buffer so it is ready for presentation using the swap chain after the rendering

		//Reference to color attachment to use for subpass
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;										//Index of the single attachment we have description we have
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//Since the attachment will be used as a color buffer, use optimal color layout

		//Description of subpass itself
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;			//Explicitly state this is a graphics subpass
		subpass.colorAttachmentCount = 1;										//This index referenced in "out" directive of fragment shader
		subpass.pColorAttachments = &colorAttachmentRef;						//Reference to color attachment

		//The render pass itself
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;										//References to the attachment and subpass
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		//Create the render pass
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, renderPass.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}

	}

	///<summary>Creates the graphics pipeline, including the shader stages, fixed function state, pipeline layout, and render pass(es)</summary>
	void createGraphicsPipeline()
	{
		//Read in shader code
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		//Create shader module references as local variables, as they are only needed for pipeline creations
		VDeleter<VkShaderModule> vertShaderModule{ device, vkDestroyShaderModule };
		VDeleter<VkShaderModule> fragShaderModule{ device, vkDestroyShaderModule };

		//Create the shader module objects
		createShaderModule(vertShaderCode, vertShaderModule);
		createShaderModule(fragShaderCode, fragShaderModule);

		//Vertex shader pipeline creation info struct
		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		//Fragment shader pipeline creation info struct
		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		//Array of both shader steps of the pipeline
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		//Struct defining creation info for the vertex shader's inputs
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

		//Struct defining how the geometry will be drawn, and if primitive restart is enabled
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		//Struct defining the viewport options, in this case making it the same dimensions as the swap chain items
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		//Struct specifying that our scissor rectangle will match the swapchain size as well
		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		//Struct defining the viewport state creation info, using the previous viewport and scissor
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		//Struct defining creation options for the rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;												//If enabled, fragments beyond near/far planes are clamped to them, instead of being discarded
		rasterizer.rasterizerDiscardEnable = VK_FALSE;										//If enabled, nothing gets passed the rasterizer stage, so no output to framebuffer
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;										//Determines if polygons are filled, drawn as lines, or drawn as points
		rasterizer.lineWidth = 1.0f;														//Sets line thickness in terms of number of fragments
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;										//Determines which, if either, face is culled
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;										//Sets which direction determines front face 
		rasterizer.depthBiasEnable = VK_FALSE;												//Settings to alter/bias the depth values
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		//Struct defining multi sampling options, which can be used for anti aliasing, however it is currently disabled
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; /// Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		//Struct defining blend options for the attached framebuffer, in this case simple alpha blending is enabled
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		
		//Struct defining options for the array of all blend option structs
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_TRUE;
		colorBlending.logicOp = VK_LOGIC_OP_AND; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		//Dynamic state structs, for options that can be changed and set at draw time
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		//Pipeline layout creation info struct, for any uniform variables in the shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0; // Optional
		pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
			pipelineLayout.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;									//Reference to the shader stages set up for the frag/vert shaders
		pipelineInfo.pVertexInputState = &vertexInputInfo;						//References to the fixed function steps
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr; // Optional
		pipelineInfo.layout = pipelineLayout;									//Vulkan object with layout information
		pipelineInfo.renderPass = renderPass;									//Reference to the render pass
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;						//Specifying that this pipeline is not derived from another pipeline
		pipelineInfo.basePipelineIndex = -1; // Optional

		//Create the graphics pipeline
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, graphicsPipeline.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}
	}

	///<summary>Iterates through the swapChainImageViews container and populates the swapChainFramebuffers container with the necessary information</summary>
	void createFrameBuffers()
	{
		//Resize the container so it will fit all of our framebuffers
		swapChainFramebuffers.resize(swapChainImageViews.size(), VDeleter<VkFramebuffer>{device, vkDestroyFramebuffer});

		//Iterate through all of the images
		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;							//Render pass this buffer will be compatible with
			framebufferInfo.attachmentCount = 1;								//Specifying 1 attached image view per buffer
			framebufferInfo.pAttachments = attachments;							//Image view to attach
			framebufferInfo.width = swapChainExtent.width;						//Width and height of image
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;											//Specify 1 image per image view

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, swapChainFramebuffers[i].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	///<summary>Creates a command pool for the graphics queue of the physical device</summary>
	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		//Command pool creation information
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;			//Specify the queue this command pool can submit command buffers on
		poolInfo.flags = 0; // Optional
		
		//Create the command pool object
		if (vkCreateCommandPool(device, &poolInfo, nullptr, commandPool.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}

	///<summary></summary>
	void createCommandBuffers()
	{
		//Resize the buffers container to match the amount of framebuffers there are
		commandBuffers.resize(swapChainFramebuffers.size());

		//Info for creating the command buffers
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;											//Specify the command pool that manages the buffer
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;								//Specifies the buffer can be submitted to a queue for execution, but not called from other buffers
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();					//Specify the amount of buffers

		//Allocate the command buffers
		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}

		//Record the command buffers
		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			//Info to begin the recording
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;				//Allows the command buffer to be resubmitted while it is awaiting execution
			beginInfo.pInheritanceInfo = nullptr; // Optional							//If this was a secondary buffer, this would be the state to inherit from the primary buffer

			vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

			//Info to start a render pass
			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;									//Reference to the render pass itself
			renderPassInfo.framebuffer = swapChainFramebuffers[i];					//The framebuffer that will be attached as a color attachment
			renderPassInfo.renderArea.offset = { 0, 0 };							//Define the offset/size of the render area, which will match the swap chain size
			renderPassInfo.renderArea.extent = swapChainExtent;
			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;										//Set the color used for VK_ATTACHMENT_LOAD_CLEAR to be black, with 100% opacity
			renderPassInfo.pClearValues = &clearColor;

			//Record the render pass command, and the commands will be embedded in the primary command buffer itself, with no secondary command buffer
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			//Bind the pipeline, specifying it is a graphics, not computer, pipeline
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			//Record the draw call, specifying vertex count, instance count, the first vertex, and the first instance
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			//Record the end of the render pass
			vkCmdEndRenderPass(commandBuffers[i]);

			//Finish recording the command
			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to record command buffer!");
			}
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
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		createCommandBuffers();
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