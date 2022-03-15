/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Image supported sample counts tests.
 *//*--------------------------------------------------------------------*/

#include "vktImageSampleCountsTests.hpp"

#include <array>
#include <string>
#include <vector>

#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vktImageLoadStoreUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktImageTexture.hpp"
#include "vktTestCaseUtil.hpp"

namespace vkt
{
namespace image
{

namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using tcu::IVec3;

// Returns true if "a" is superset of "b"
bool isSuperset (VkSampleCountFlags a, VkSampleCountFlags b)
{
	return (a & b) == b;
}

VkSampleCountFlags SampleCountTestInstance::getColorSampleCounts (
    const VkPhysicalDeviceProperties2&            physicalDeviceProperties,
    const vk::VkPhysicalDeviceVulkan12Properties& physicalDeviceProperties12)
{
	if (!isCompressedFormat(m_caseDef.format))
	{
		// If usage includes VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and format is a floating- or fixed-point color format,
		// a superset of VkPhysicalDeviceLimits::framebufferColorSampleCounts is expected
		if (vk::isFloatFormat(m_caseDef.format) || isSnormFormat(m_caseDef.format) || isUnormFormat(m_caseDef.format))
		{
			return physicalDeviceProperties.properties.limits.framebufferColorSampleCounts;
		}

		// If usage includes VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and format is an integer format,
		// a superset of VkPhysicalDeviceVulkan12Properties::framebufferIntegerColorSampleCounts is expected
		else if (vk::isIntFormat(m_caseDef.format) || vk::isUintFormat(m_caseDef.format))
		{
			return physicalDeviceProperties12.framebufferIntegerColorSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags SampleCountTestInstance::getDepthStencilSampleCounts (
    const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	if (!isCompressedFormat(m_caseDef.format))
	{
		auto format = mapVkFormat(m_caseDef.format);
		// If usage includes VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format includes a depth aspect,
		// a superset of VkPhysicalDeviceLimits::framebufferDepthSampleCounts is expected
		if (format.order == tcu::TextureFormat::D)
		{
			return physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts;
		}
		// If usage includes VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format includes a stencil aspect,
		// a superset of VkPhysicalDeviceLimits::framebufferStencilSampleCounts is expected
		else if (format.order == tcu::TextureFormat::S)
		{
			return physicalDeviceProperties.properties.limits.framebufferStencilSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags SampleCountTestInstance::getSampledSampleCounts (
    const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	if (!isCompressedFormat(m_caseDef.format) && !isYCbCrFormat(m_caseDef.format))
	{
		auto format = mapVkFormat(m_caseDef.format);

		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format includes a color aspect,
		// a superset of VkPhysicalDeviceLimits::sampledImageColorSampleCounts is expected
		if (format.order != tcu::TextureFormat::D && format.order != tcu::TextureFormat::DS &&
		    format.order != tcu::TextureFormat::S)
		{
			return physicalDeviceProperties.properties.limits.sampledImageColorSampleCounts;
		}
		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format includes a depth aspect,
		// a superset of VkPhysicalDeviceLimits::sampledImageDepthSampleCounts is expected
		else if (format.order == tcu::TextureFormat::D || format.order == tcu::TextureFormat::DS)
		{
			return physicalDeviceProperties.properties.limits.sampledImageDepthSampleCounts;
		}

		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format is an integer format,
		// a superset of VkPhysicalDeviceLimits::sampledImageIntegerSampleCounts is expected
		if (vk::isIntFormat(m_caseDef.format) || vk::isUintFormat(m_caseDef.format))
		{
			return physicalDeviceProperties.properties.limits.sampledImageIntegerSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags SampleCountTestInstance::getStorageSampleCounts (
    const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	return physicalDeviceProperties.properties.limits.storageImageSampleCounts;
}

void SampleCountTest::checkSupport (Context& ctx) const
{
	VkImageFormatProperties imageFormatProperties;
	VkResult                imageFormatResult = ctx.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	                   ctx.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, m_caseDef.usageFlags, 0,
	                   &imageFormatProperties);

	if (imageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
	{
		TCU_THROW(NotSupportedError, "Format is not supported");
	}
}

SampleCountTestInstance::SampleCountTestInstance(Context& context, const CaseDef& caseDef, SampleCountsSubtests subtest)
    : TestInstance(context), m_caseDef(caseDef), m_subtest(subtest)
{
}

bool SampleCountTestInstance::checkUsageFlags ()
{
	vk::VkFormatProperties formatProperties;
	m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), m_caseDef.format,
	                                                                   &formatProperties);

	vk::VkPhysicalDeviceVulkan12Properties physicalDeviceProperties12{};
	physicalDeviceProperties12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	physicalDeviceProperties12.pNext = nullptr;

	vk::VkPhysicalDeviceProperties2 physicalDeviceProperties{};
	physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties.pNext = &physicalDeviceProperties12;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(),
	                                                              &physicalDeviceProperties);

	VkImageFormatProperties imageFormatProperties;
	VkResult                imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	                   m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling,
	                   m_caseDef.usageFlags, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	std::vector<VkSampleCountFlags> expectedSupersets;

	if (!(formatProperties.optimalTilingFeatures &
	      (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)))
	{
		return imageFormatProperties.sampleCounts == VK_SAMPLE_COUNT_1_BIT;
	}

	if (m_caseDef.usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		expectedSupersets.push_back(getColorSampleCounts(physicalDeviceProperties, physicalDeviceProperties12));
	}

	if (m_caseDef.usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		expectedSupersets.push_back(getDepthStencilSampleCounts(physicalDeviceProperties));
	}

	if (m_caseDef.usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
	{
		expectedSupersets.push_back(getSampledSampleCounts(physicalDeviceProperties));
	}

	if (m_caseDef.usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		expectedSupersets.push_back(getStorageSampleCounts(physicalDeviceProperties));
	}

	// If none of the bits described above are set in usage, then there is no corresponding limit in VkPhysicalDeviceLimits
	// In this case, sampleCounts must include at least VK_SAMPLE_COUNT_1_BIT
	if (expectedSupersets.empty())
	{
		return isSuperset(imageFormatProperties.sampleCounts, VK_SAMPLE_COUNT_1_BIT);
	}

	// If multiple bits are set in usage, sampleCounts will be the intersection of the per-usage values described above
	VkSampleCountFlags expectedFlags = expectedSupersets[0];
	for (VkSampleCountFlags setFlags : expectedSupersets)
	{
		expectedFlags &= setFlags;
	}

	return isSuperset(imageFormatProperties.sampleCounts, expectedFlags);
}

bool SampleCountTestInstance::checkYCBCRConversion ()
{
	VkImageFormatProperties imageFormatProperties;
	VkResult                imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	                   m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, 0, 0,
	                   &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS || imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkLinearTilingAndNot2DImageType ()
{
	VkImageFormatProperties imageFormatProperties;
	const VkResult          imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	             m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, 0, 0,
	             &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	if (imageFormatProperties.sampleCounts == VK_SAMPLE_COUNT_1_BIT &&
	    (m_caseDef.imageTiling == VK_IMAGE_TILING_LINEAR || // tiling is VK_IMAGE_TILING_LINEAR
	     m_caseDef.imageType != VK_IMAGE_TYPE_2D))          // type is not VK_IMAGE_TYPE_2D
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool SampleCountTestInstance::checkCubeCompatible ()
{
	VkImageFormatProperties imageFormatProperties;
	VkResult                imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	                   m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, 0,
	                   VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS || imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkOptimalTilingFeatures ()
{
	vk::VkFormatProperties formatProperties;
	m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), m_caseDef.format,
	                                                                   &formatProperties);

	VkImageFormatProperties imageFormatProperties;
	VkResult                imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	                   m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, 0, 0,
	                   &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	// Neither the VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT flag nor
	// the VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT flag in
	// VkFormatProperties::optimalTilingFeatures returned by vkGetPhysicalDeviceFormatProperties is set
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) &&
	    !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
	    imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkOneSampleCountPresent ()
{
	VkImageFormatProperties imageFormatProperties;

	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
	    m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling, 0, 0,
	    &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	// If none of the bits described above are set in usage, then there is no corresponding limit in
	// VkPhysicalDeviceLimits. In this case, sampleCounts must include at least VK_SAMPLE_COUNT_1_BIT.
	if (!(imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
	{
		return false;
	}

	return true;
}

tcu::TestStatus SampleCountTestInstance::iterate ()
{
	bool success = false;

	switch (m_subtest)
	{
	case LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE:
		success = checkLinearTilingAndNot2DImageType();
		break;
	case CUBE_COMPATIBLE_SUBTEST:
		success = checkCubeCompatible();
		break;
	case OPTIMAL_TILING_FEATURES_SUBTEST:
		success = checkOptimalTilingFeatures();
		break;
	case YCBCR_CONVERSION_SUBTEST:
		success = checkYCBCRConversion();
		break;
	case USAGE_FLAGS_SUBTEST:
		success = checkUsageFlags();
		break;
	case ONE_SAMPLE_COUNT_PRESENT_SUBTEST:
		success = checkOneSampleCountPresent();
		break;
	}

	return success ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("FAILED");
}

SampleCountTest::SampleCountTest(tcu::TestContext& testCtx, const std::string& name, const std::string& description,
                                 const CaseDef& caseDef, SampleCountTestInstance::SampleCountsSubtests subtest)
    : TestCase(testCtx, name, description), m_caseDef(caseDef), m_subtest(subtest)
{
}

vkt::TestInstance* SampleCountTest::createInstance (Context& context) const
{
	return new SampleCountTestInstance(context, m_caseDef, m_subtest);
}

} // namespace

static void addUsageFlagsSubtests (tcu::TestContext& testCtx, const std::string& samplesCaseName,
                                   const CaseDef& caseDef, tcu::TestCaseGroup* group)
{
	const std::array<VkImageUsageFlagBits, 4> usageFlags = {
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_USAGE_STORAGE_BIT,
	};

	for (unsigned flagsCombination = 0; flagsCombination < (1 << usageFlags.size()); flagsCombination++)
	{
		VkImageUsageFlags usage = 0;
		for (unsigned flagIdx = 0; flagIdx < usageFlags.size(); flagIdx++)
		{
			if ((flagsCombination >> flagIdx) & 1)
			{
				usage |= usageFlags[flagIdx];
			}
		}

		std::string caseName = samplesCaseName + "_USAGE_FLAGS";
		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			caseName += "_COLOR";
		}

		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			caseName += "_DEPTH";
		}

		if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			caseName += "_SAMPLED";
		}

		if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			caseName += "_STORAGE";
		}

		CaseDef newCaseDef = { caseDef.format, caseDef.imageType, caseDef.imageTiling, usage };

		group->addChild(
		    new SampleCountTest(testCtx, caseName, "", newCaseDef, SampleCountTestInstance::USAGE_FLAGS_SUBTEST));
	}
}

static std::vector<VkFormat> enumerateAllFormatsToTest ()
{
	const std::array<std::pair<VkFormat, VkFormat>, 6> allFormatsRanges = {
		std::make_pair(VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_ASTC_12x12_SRGB_BLOCK),
		std::make_pair(VK_FORMAT_G8B8G8R8_422_UNORM, VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM),
		std::make_pair(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM, VK_FORMAT_G16_B16R16_2PLANE_444_UNORM),
		std::make_pair(VK_FORMAT_A4R4G4B4_UNORM_PACK16, VK_FORMAT_A4B4G4R4_UNORM_PACK16),
		std::make_pair(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK, VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK),
		std::make_pair(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG),
	};

	std::vector<VkFormat> formats;

	for (auto range : allFormatsRanges)
	{
		for (VkFormat format = range.first; format <= range.second; format = (VkFormat)(format + 1))
		{
			formats.push_back(format);
		}
	}

	return formats;
}

tcu::TestCaseGroup* createImageSampleCountsTests (tcu::TestContext& testCtx)
{
	const std::array<VkImageTiling, 2> imageTilings = {
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_TILING_LINEAR,
	};

	const std::array<ImageType, 3> imageTypes = {
		IMAGE_TYPE_1D,
		IMAGE_TYPE_2D,
		IMAGE_TYPE_3D,
	};

	MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "sample_counts", "Image sample counts"));

	for (ImageType imageType : imageTypes)
	{
		const std::string typeGroupName = getImageTypeName(imageType);

		MovePtr<tcu::TestCaseGroup> imageViewGroup(new tcu::TestCaseGroup(testCtx, typeGroupName.c_str(), ""));

		for (VkImageTiling imageTiling : imageTilings)
		{
			const std::string tilingGroupName = getImageTilingName(imageTiling);

			MovePtr<tcu::TestCaseGroup> tilingGroup(new tcu::TestCaseGroup(testCtx, tilingGroupName.c_str(), ""));

			for (VkFormat imageFormat : enumerateAllFormatsToTest())
			{
				const std::string formatStr = getFormatShortString(imageFormat);

				const CaseDef caseDef = { imageFormat, mapImageType(imageType), imageTiling, 0 };

				if (caseDef.imageType == VK_IMAGE_TYPE_2D && caseDef.imageTiling == VK_IMAGE_TILING_OPTIMAL)
				{
					tilingGroup.get()->addChild(new SampleCountTest(testCtx, formatStr + "_CUBE_COMPATIBLE_SUBTEST", "",
					                                                caseDef,
					                                                SampleCountTestInstance::CUBE_COMPATIBLE_SUBTEST));

					tilingGroup.get()->addChild(
					    new SampleCountTest(testCtx, formatStr + "_OPTIMAL_TILING_FEATURES_SUBTEST", "", caseDef,
					                        SampleCountTestInstance::OPTIMAL_TILING_FEATURES_SUBTEST));

					if (isYCbCrFormat(caseDef.format))
					{
						tilingGroup.get()->addChild(
						    new SampleCountTest(testCtx, formatStr + "_YCBCR_CONVERSION_SUBTEST", "", caseDef,
						                        SampleCountTestInstance::YCBCR_CONVERSION_SUBTEST));
					}

					addUsageFlagsSubtests(testCtx, formatStr, caseDef, tilingGroup.get());

					tilingGroup.get()->addChild(
					    new SampleCountTest(testCtx, formatStr + "_ONE_SAMPLE_COUNT_PRESENT_SUBTEST", "", caseDef,
					                        SampleCountTestInstance::ONE_SAMPLE_COUNT_PRESENT_SUBTEST));
				}
				else
				{
					tilingGroup.get()->addChild(
					    new SampleCountTest(testCtx, formatStr + "_LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE_SUBTEST", "",
					                        caseDef, SampleCountTestInstance::LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE));
				}
			}

			imageViewGroup->addChild(tilingGroup.release());
		}

		testGroup->addChild(imageViewGroup.release());
	}

	return testGroup.release();
}

} // namespace image

} // namespace vkt
