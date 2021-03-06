/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkData.h"
#include "SkDiscardableMemoryPool.h"
#include "SkImage.h"
#include "SkImageEncoder.h"
#include "SkImageGenerator.h"
#include "SkMakeUnique.h"
#include "SkResourceCache.h"
#include "SkStream.h"
#include "SkUtils.h"

#include "Test.h"

class TestImageGenerator : public SkImageGenerator {
public:
    enum TestType {
        kFailGetPixels_TestType,
        kSucceedGetPixels_TestType,
        kLast_TestType = kSucceedGetPixels_TestType
    };
    static int Width() { return 10; }
    static int Height() { return 10; }
    // value choosen so that there is no loss when converting to to RGB565 and back
    static SkColor Color() { return 0xff10345a; }
    static SkPMColor PMColor() { return SkPreMultiplyColor(Color()); }

    TestImageGenerator(TestType type, skiatest::Reporter* reporter,
                       SkColorType colorType = kN32_SkColorType)
    : INHERITED(GetMyInfo(colorType)), fType(type), fReporter(reporter) {
        SkASSERT((fType <= kLast_TestType) && (fType >= 0));
    }
    virtual ~TestImageGenerator() { }

protected:
    static SkImageInfo GetMyInfo(SkColorType colorType) {
        return SkImageInfo::Make(TestImageGenerator::Width(), TestImageGenerator::Height(),
                                 colorType, kOpaque_SkAlphaType);
    }

    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     SkPMColor ctable[], int* ctableCount) override {
        REPORTER_ASSERT(fReporter, pixels != nullptr);
        REPORTER_ASSERT(fReporter, rowBytes >= info.minRowBytes());
        if (fType != kSucceedGetPixels_TestType) {
            return false;
        }
        if (info.colorType() != kN32_SkColorType && info.colorType() != getInfo().colorType()) {
            return false;
        }
        char* bytePtr = static_cast<char*>(pixels);
        switch (info.colorType()) {
            case kN32_SkColorType:
                for (int y = 0; y < info.height(); ++y) {
                    sk_memset32((uint32_t*)bytePtr,
                                TestImageGenerator::PMColor(), info.width());
                    bytePtr += rowBytes;
                }
                break;
            case kIndex_8_SkColorType:
                *ctableCount = 1;
                ctable[0] = TestImageGenerator::PMColor();
                for (int y = 0; y < info.height(); ++y) {
                    memset(bytePtr, 0, info.width());
                    bytePtr += rowBytes;
                }
                break;
            case kRGB_565_SkColorType:
                for (int y = 0; y < info.height(); ++y) {
                    sk_memset16((uint16_t*)bytePtr,
                        SkPixel32ToPixel16(TestImageGenerator::PMColor()), info.width());
                    bytePtr += rowBytes;
                }
                break;
            default:
                return false;
        }
        return true;
    }

private:
    const TestType fType;
    skiatest::Reporter* const fReporter;

    typedef SkImageGenerator INHERITED;
};

////////////////////////////////////////////////////////////////////////////////

DEF_TEST(Image_NewFromGenerator, r) {
    const TestImageGenerator::TestType testTypes[] = {
        TestImageGenerator::kFailGetPixels_TestType,
        TestImageGenerator::kSucceedGetPixels_TestType,
    };
    const SkColorType testColorTypes[] = {
        kN32_SkColorType,
        kIndex_8_SkColorType,
        kRGB_565_SkColorType
    };
    for (size_t i = 0; i < SK_ARRAY_COUNT(testTypes); ++i) {
        TestImageGenerator::TestType test = testTypes[i];
        for (const SkColorType testColorType : testColorTypes) {
            auto gen = skstd::make_unique<TestImageGenerator>(test, r, testColorType);
            sk_sp<SkImage> image(SkImage::MakeFromGenerator(std::move(gen)));
            if (nullptr == image) {
                ERRORF(r, "SkImage::NewFromGenerator unexpecedly failed ["
                    SK_SIZE_T_SPECIFIER "]", i);
                continue;
            }
            REPORTER_ASSERT(r, TestImageGenerator::Width() == image->width());
            REPORTER_ASSERT(r, TestImageGenerator::Height() == image->height());
            REPORTER_ASSERT(r, image->isLazyGenerated());

            SkBitmap bitmap;
            bitmap.allocN32Pixels(TestImageGenerator::Width(), TestImageGenerator::Height());
            SkCanvas canvas(bitmap);
            const SkColor kDefaultColor = 0xffabcdef;
            canvas.clear(kDefaultColor);
            canvas.drawImage(image, 0, 0, nullptr);
            if (TestImageGenerator::kSucceedGetPixels_TestType == test) {
                REPORTER_ASSERT(
                    r, TestImageGenerator::Color() == bitmap.getColor(0, 0));
            }
            else {
                REPORTER_ASSERT(r, kDefaultColor == bitmap.getColor(0, 0));
            }
        }
    }
}
