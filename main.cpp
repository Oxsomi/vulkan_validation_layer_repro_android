

#include <android_native_app_glue.h>

#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <vector>

#include <android/log.h>
#include <vulkan/vulkan.h>

typedef uint64_t U64;
typedef uint32_t U32;
typedef uint8_t U8;
typedef float F32;
typedef int32_t I32;
typedef U64 Ns;
typedef char C8;

#define SECOND 1000000000
#define MU 1000

bool Thread_sleep(Ns ns) {

	struct timespec remaining, request = { (time_t)(ns / SECOND), (long)(ns % SECOND) };

	while(true) {

		errno = 0;
		int ret = nanosleep(&request, &remaining);

		if(!ret)
			return true;

		if(errno == EINTR)
			continue;

		return false;
	}
}

bool finalized = false;

void AWindow_onAppCmd(struct android_app *app, I32 cmd) {
	if(cmd == APP_CMD_INIT_WINDOW)
		finalized = true;
}

VkBool32 onDebugReport(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	U64 object,
	U64 location,
	I32 messageCode,
	const C8 *layerPrefix,
	const C8 *message,
	void *userData
) {

	(void)messageCode;
	(void)object;
	(void)location;
	(void)objectType;
	(void)userData;
	(void)layerPrefix;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "%s", message);

	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		__android_log_print(ANDROID_LOG_WARN, "OxC3", "%s", message);

	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		__android_log_print(ANDROID_LOG_INFO, "OxC3", "%s", message);

	else __android_log_print(ANDROID_LOG_DEBUG, "OxC3", "%s", message);

	return VK_FALSE;
}

void android_main(android_app *app) {
	
	__android_log_print(ANDROID_LOG_INFO, "OxC3", "Start");

	int ident, events;
	struct android_poll_source *source;
	
	app->onAppCmd = AWindow_onAppCmd;

repeat:
	while((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
		if(source)
			source->process(app, source);
	}

	//In case of initialization, we have to wait until the surface is ready.
	//Afterwards, we can continue

	if(ident == ALOOPER_POLL_TIMEOUT && !finalized) {
		Thread_sleep(100 * MU);
		goto repeat;
	}
	
	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App create instance");

	//Now do some vulkan
	
	VkApplicationInfo application = (VkApplicationInfo) {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Test app",
		.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
		.pEngineName = "OxC3",
		.engineVersion = VK_MAKE_VERSION(0, 2, 95),
		.apiVersion = VK_MAKE_VERSION(1, 1, 0)
	};

	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	const char *ext[] = {
		"VK_EXT_debug_report"
	};

	VkInstanceCreateInfo instanceInfo = (VkInstanceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &application,
		.enabledLayerCount = sizeof(layers) / sizeof(layers[0]),
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = sizeof(ext) / sizeof(ext[0]),
		.ppEnabledExtensionNames = ext
	};

	VkValidationFeatureEnableEXT enables[] = {
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
		VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
		VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
	};

	VkValidationFeaturesEXT valFeat = (VkValidationFeaturesEXT) {
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.enabledValidationFeatureCount = (U32) (sizeof(enables) / sizeof(enables[0])),
		.pEnabledValidationFeatures = enables
	};

	instanceInfo.pNext = &valFeat;

	VkInstance instance = NULL;
	VkResult res = vkCreateInstance(&instanceInfo, NULL, &instance);

	if(res != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create instance %i", res);
		return;
	}

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App get debug report callback ext");

	PFN_vkVoidFunction v = vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

	if(!v) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "No debug callback");
		return;
	}

	VkDebugReportCallbackEXT debugReportCallback = NULL;
	VkDebugReportCallbackCreateInfoEXT callbackInfo = (VkDebugReportCallbackCreateInfoEXT) {

		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,

		.flags =
			VK_DEBUG_REPORT_DEBUG_BIT_EXT |
			VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
			VK_DEBUG_REPORT_ERROR_BIT_EXT |
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,

		.pfnCallback = (PFN_vkDebugReportCallbackEXT) onDebugReport
	};

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App create debug callback");

	res = ((PFN_vkCreateDebugReportCallbackEXT)v)(
		instance, &callbackInfo, NULL, &debugReportCallback
	);

	if(res != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create debug callback");
		return;
	}

	U32 deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	std::vector<VkPhysicalDevice> devices;
	devices.resize(deviceCount);

	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	F32 prio = 1;
	VkDeviceQueueCreateInfo queue = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = 0,
		.queueCount = 1,
		.pQueuePriorities = &prio
	};
	
	const C8 *deviceExt[] = {
		"VK_EXT_descriptor_indexing"
	};

	VkPhysicalDeviceFeatures features = (VkPhysicalDeviceFeatures) { 0 };

	VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.features = features
	};

	void **currPNext = &features2.pNext;
	
	#define bindNextVkStruct(T, condition, ...)	\
		T tmp##T = __VA_ARGS__;				\
											\
		if(condition) {						\
			*currPNext = &tmp##T;			\
			currPNext = &tmp##T.pNext;		\
		}

	bindNextVkStruct(
		VkPhysicalDeviceDescriptorIndexingFeatures,
		true,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
			.shaderUniformTexelBufferArrayDynamicIndexing = true,
			.shaderStorageTexelBufferArrayDynamicIndexing = true,
			.shaderUniformBufferArrayNonUniformIndexing = true,
			.shaderSampledImageArrayNonUniformIndexing = true,
			.shaderStorageBufferArrayNonUniformIndexing = true,
			.shaderStorageImageArrayNonUniformIndexing = true,
			.shaderUniformTexelBufferArrayNonUniformIndexing = true,
			.shaderStorageTexelBufferArrayNonUniformIndexing = true,
			.descriptorBindingSampledImageUpdateAfterBind = true,
			.descriptorBindingStorageImageUpdateAfterBind = true,
			.descriptorBindingStorageBufferUpdateAfterBind = true,
			.descriptorBindingUniformTexelBufferUpdateAfterBind = true,
			.descriptorBindingStorageTexelBufferUpdateAfterBind = true,
			.descriptorBindingUpdateUnusedWhilePending = true,
			.descriptorBindingPartiallyBound = true,
			.descriptorBindingVariableDescriptorCount = true,
			.runtimeDescriptorArray = true
		}
	)

	VkDeviceCreateInfo deviceInfo = (VkDeviceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.queueCreateInfoCount = (U32) 1,
		.pQueueCreateInfos = &queue,
		.enabledExtensionCount = sizeof(deviceExt) / sizeof(deviceExt[0]),
		.ppEnabledExtensionNames = deviceExt
	};

	VkDevice device = NULL;
	vkCreateDevice(devices[0], &deviceInfo, NULL, &device);

	//Load shader

	std::vector<U8> buf;

	AAssetManager *assetManager = app->activity->assetManager;
	AAsset *asset = AAssetManager_open(assetManager, "indirect_prepare.spv", AASSET_MODE_BUFFER);

	if (!asset) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed load asset");
		return;
	}

	buf.resize(AAsset_getLength64(asset));

	int r = AAsset_read(asset, buf.data(), buf.size());

	if (r < 0 || (U32)r != buf.size()) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed read asset");
		return;
	}

	//Create layout

	bool hasRt = false;

	typedef enum EDescriptorSetType {

		EDescriptorSetType_Sampler,
		EDescriptorSetType_Resources,
		EDescriptorSetType_CBuffer0,
		EDescriptorSetType_CBuffer1,		//Versioning
		EDescriptorSetType_CBuffer2,

		EDescriptorSetType_Count,
		EDescriptorSetType_UniqueLayouts = EDescriptorSetType_CBuffer1

	} EDescriptorSetType;

	typedef enum EDescriptorType {

		EDescriptorType_Texture2D,
		EDescriptorType_TextureCube,
		EDescriptorType_Texture3D,
		EDescriptorType_Buffer,

		EDescriptorType_RWBuffer,
		EDescriptorType_RWTexture3D,
		EDescriptorType_RWTexture3Ds,
		EDescriptorType_RWTexture3Df,
		EDescriptorType_RWTexture3Di,
		EDescriptorType_RWTexture3Du,
		EDescriptorType_RWTexture2D,
		EDescriptorType_RWTexture2Ds,
		EDescriptorType_RWTexture2Df,
		EDescriptorType_RWTexture2Di,
		EDescriptorType_RWTexture2Du,

		//Both are allocated at 0xF << 17 but they reserve 4 more bits after to indicate the extended type.
		//This allow us up to 32 descriptor types

		EDescriptorType_ExtendedType,
		EDescriptorType_Sampler = EDescriptorType_ExtendedType,
		EDescriptorType_TLASExt,

		EDescriptorType_ResourceCount		//Count of totally accessible resources (without cbuffer)

	} EDescriptorType;

	typedef enum EDescriptorTypeCount {

		EDescriptorTypeCount_Texture2D		= 131072,
		EDescriptorTypeCount_TextureCube	= 32768,
		EDescriptorTypeCount_Texture3D		= 32768,
		EDescriptorTypeCount_Buffer			= 131072,
		EDescriptorTypeCount_RWBuffer		= 131072,
		EDescriptorTypeCount_RWTexture3D	= 8192,
		EDescriptorTypeCount_RWTexture3Ds	= 8192,
		EDescriptorTypeCount_RWTexture3Df	= 32768,
		EDescriptorTypeCount_RWTexture3Di	= 8192,
		EDescriptorTypeCount_RWTexture3Du	= 8192,
		EDescriptorTypeCount_RWTexture2D	= 65536,
		EDescriptorTypeCount_RWTexture2Ds	= 8192,
		EDescriptorTypeCount_RWTexture2Df	= 65536,
		EDescriptorTypeCount_RWTexture2Di	= 16384,
		EDescriptorTypeCount_RWTexture2Du	= 16384,
		EDescriptorTypeCount_Sampler		= 2048,		//All samplers
		EDescriptorTypeCount_TLASExt		= 16,

		EDescriptorTypeCount_Textures		=
		EDescriptorTypeCount_Texture2D + EDescriptorTypeCount_Texture3D + EDescriptorTypeCount_TextureCube,

		EDescriptorTypeCount_RWTextures2D	=
		EDescriptorTypeCount_RWTexture2D  + EDescriptorTypeCount_RWTexture2Ds +	EDescriptorTypeCount_RWTexture2Df +
		EDescriptorTypeCount_RWTexture2Du + EDescriptorTypeCount_RWTexture2Di,

		EDescriptorTypeCount_RWTextures3D	=
		EDescriptorTypeCount_RWTexture3D  + EDescriptorTypeCount_RWTexture3Ds +	EDescriptorTypeCount_RWTexture3Df +
		EDescriptorTypeCount_RWTexture3Du + EDescriptorTypeCount_RWTexture3Di,

		EDescriptorTypeCount_RWTextures		= EDescriptorTypeCount_RWTextures2D + EDescriptorTypeCount_RWTextures3D,

		EDescriptorTypeCount_SSBO			=
		EDescriptorTypeCount_Buffer + EDescriptorTypeCount_RWBuffer + EDescriptorTypeCount_TLASExt

	} EDescriptorTypeCount;

	static const U32 descriptorTypeCount[] = {
		EDescriptorTypeCount_Texture2D,
		EDescriptorTypeCount_TextureCube,
		EDescriptorTypeCount_Texture3D,
		EDescriptorTypeCount_Buffer,
		EDescriptorTypeCount_RWBuffer,
		EDescriptorTypeCount_RWTexture3D,
		EDescriptorTypeCount_RWTexture3Ds,
		EDescriptorTypeCount_RWTexture3Df,
		EDescriptorTypeCount_RWTexture3Di,
		EDescriptorTypeCount_RWTexture3Du,
		EDescriptorTypeCount_RWTexture2D,
		EDescriptorTypeCount_RWTexture2Ds,
		EDescriptorTypeCount_RWTexture2Df,
		EDescriptorTypeCount_RWTexture2Di,
		EDescriptorTypeCount_RWTexture2Du,
		EDescriptorTypeCount_Sampler,
		EDescriptorTypeCount_TLASExt
	};

	VkDescriptorSetLayout setLayouts[EDescriptorSetType_UniqueLayouts] = { NULL };

	for (U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i) {

		VkDescriptorSetLayoutBinding bindings[EDescriptorType_ResourceCount - 1];
		U8 bindingCount = 0;

		if (i == EDescriptorSetType_Resources) {

			for(U32 j = EDescriptorType_Texture2D; j < EDescriptorType_ResourceCount; ++j) {

				if(j == EDescriptorType_Sampler)
					continue;

				if(j == EDescriptorType_TLASExt && !hasRt)
					continue;

				U32 id = j;

				if(j > EDescriptorType_Sampler)
					--id;

				if(j > EDescriptorType_TLASExt && !hasRt)
					--id;

				bindings[id] = (VkDescriptorSetLayoutBinding) {
					.binding = id,
					.descriptorType =
						j == EDescriptorType_TLASExt ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : (
							j < EDescriptorType_Buffer ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : (
								j <= EDescriptorType_RWBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER :
								VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
							)
						),
					.descriptorCount = descriptorTypeCount[j],
					.stageFlags = VK_SHADER_STAGE_ALL
				};
			}

			bindingCount = EDescriptorType_ResourceCount - 1 - !hasRt;
		}

		else {

			bool isSampler = i == EDescriptorSetType_Sampler;

			bindings[0] = (VkDescriptorSetLayoutBinding) {
				.descriptorType = isSampler ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = (U32)(isSampler ? EDescriptorTypeCount_Sampler : 1),
				.stageFlags = VK_SHADER_STAGE_ALL
			};

			bindingCount = 1;
		}

		//One binding per set.

		VkDescriptorBindingFlags flags[EDescriptorType_ResourceCount - 1] = {
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
		};

		for(U32 j = 1; j < EDescriptorType_ResourceCount - 1; ++j)
			flags[j] = flags[0];

		if (i >= EDescriptorSetType_CBuffer0 && i <= EDescriptorSetType_CBuffer2)	//We don't touch CBuffer after bind
			flags[0] &=~ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo partiallyBound = (VkDescriptorSetLayoutBindingFlagsCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = bindingCount,
			.pBindingFlags = flags
		};

		VkDescriptorSetLayoutCreateInfo setInfo = (VkDescriptorSetLayoutCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &partiallyBound,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = bindingCount,
			.pBindings = bindings
		};

		res = vkCreateDescriptorSetLayout(device, &setInfo, NULL, &setLayouts[i]);

		if(res != VK_SUCCESS) {
			__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create descriptor set layout");
			return;
		}
	}

	VkPipelineLayoutCreateInfo layoutInfo = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = EDescriptorSetType_UniqueLayouts,
		.pSetLayouts = setLayouts
	};

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App create pipeline layout");

	VkPipelineLayout defaultLayout = NULL;
	res = vkCreatePipelineLayout(device, &layoutInfo, NULL, &defaultLayout);

	if(res != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create compute pipeline layout");
		return;
	}

	//Create shader bin

	VkComputePipelineCreateInfo pipelineInfo = (VkComputePipelineCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.pName = "main"
	},
		.layout = defaultLayout
	};

	VkShaderModuleCreateInfo info = (VkShaderModuleCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (U32) buf.size(),
		.pCode = (const U32*) buf.data()
	};

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App create shader module");

	res = vkCreateShaderModule(device, &info, NULL, &pipelineInfo.stage.module);

	if(res != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create compute pipeline module");
		return;
	}

	//Expected; returning Success or error creating compute pipeline
	//Real result: Crash

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App create compute pipeline");

	VkPipeline handle = NULL;
	res = vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &handle);

	if(res != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, "OxC3", "App failed to create compute pipeline");
		return;
	}

	__android_log_print(ANDROID_LOG_INFO, "OxC3", "App succeeded to create compute pipeline");
}