/*
 *  DashVideoSegmenterHEVC.cpp - DASH HEVC video stream segmenter
 *  Copyright (C) 2014  Fundació i2CAT, Internet i Innovació digital a Catalunya
 *
 *  This file is part of liveMediaStreamer.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors:  Gerard Castillo <gerard.castillo@i2cat.net>
 *
 */

#include "DashVideoSegmenterHEVC.hh"

DashVideoSegmenterHEVC::DashVideoSegmenterHEVC(std::chrono::seconds segDur) : 
DashVideoSegmenter(segDur, VIDEO_CODEC_HEVC), newFrame(true)
{
    vFrame = InterleavedVideoFrame::createNew(H265, 5000000);

}

DashVideoSegmenterHEVC::~DashVideoSegmenterHEVC()
{

}

uint8_t DashVideoSegmenterHEVC::generateContext()
{
    return generate_context(&dashContext, VIDEO_TYPE_HEVC);
}

void DashVideoSegmenterHEVC::updateMetadata()
{
    if (vps.empty() || sps.empty() || pps.empty()) {
        return;
    }

    createMetadata();
    vps.clear();
    sps.clear();
    pps.clear();
    return;
}

bool DashVideoSegmenterHEVC::flushDashContext()
{
    if (!dashContext) {
        return false;
    }

    context_refresh(&dashContext, VIDEO_TYPE_HEVC);
    return true;
}

VideoFrame* DashVideoSegmenterHEVC::parseNal(VideoFrame* nal)
{
    int startCodeOffset;
    unsigned char* nalData;
    int nalDataLength;
    unsigned char nalType;

    startCodeOffset = detectStartCode(nal->getDataBuf());
    nalData = nal->getDataBuf() + startCodeOffset;
    nalDataLength = nal->getLength() - startCodeOffset;

    if (!nalData || nalDataLength <= 0) {
        utils::errorMsg("Error parsing NAL: invalid data pointer or length");
        return NULL;
    }

    nalType = (nalData[0] & H265_NALU_TYPE_MASK) >> 1;

    switch (nalType) {
        case VPS:
            saveVPS(nalData, nalDataLength);
            isIntra = false;
            break;
        case SPS_HEVC:
            saveSPS(nalData, nalDataLength);
            isIntra = false;
            break;
        case PPS_HEVC:
            savePPS(nalData, nalDataLength);
            isIntra = false;
            break;
        case PREFIX_SEI_NUT:
        case SUFFIX_SEI_NUT:
            isIntra = false;
            break;
        case AUD_HEVC:
            newFrame = true;
            return vFrame;
        case IDR1:
        case IDR2:
        case CRA:
            isIntra = true;
            break;
        case NON_TSA_STSA_0:
        case NON_TSA_STSA_1:
            isIntra = false;
            break;
        default:
            utils::errorMsg("Error parsing NAL: NalType " + std::to_string(nalType) + " not contemplated");
            return NULL;
    }

    if (nalType == IDR1 || nalType == IDR2 || nalType == CRA || nalType == NON_TSA_STSA_0 || nalType == NON_TSA_STSA_1) {
        
        if (newFrame) {
            vFrame->setLength(0);
            newFrame = false;
        }

        if (!appendNalToFrame(vFrame, nalData, nalDataLength, nal->getWidth(), nal->getHeight(), nal->getPresentationTime())) {
            utils::errorMsg("[DashVideoSegmenterHEVC::parseNal] Error appending NAL to frame");
            return NULL;
        }
    }

    return NULL;
}

void DashVideoSegmenterHEVC::saveVPS(unsigned char* data, int dataLength)
{
    vps.clear();
    vps.insert(vps.begin(), data, data + dataLength);
}

void DashVideoSegmenterHEVC::saveSPS(unsigned char* data, int dataLength)
{
    sps.clear();
    sps.insert(sps.begin(), data, data + dataLength);
}

void DashVideoSegmenterHEVC::savePPS(unsigned char* data, int dataLength)
{
    pps.clear();
    pps.insert(pps.begin(), data, data + dataLength);
}

void DashVideoSegmenterHEVC::createMetadata()
{
    int vpsLength = vps.size();
    int spsLength = sps.size();
    int ppsLength = pps.size();

    metadata.clear();

    metadata.insert(metadata.end(), H265_METADATA_VERSION_FLAG);
    metadata.insert(metadata.end(), H265_METADATA_CONFIG_FLAGS);
    metadata.insert(metadata.end(), H265_METADATA_PROFILE_COMPATIBILITY_FLAGS);
    metadata.insert(metadata.end(), H265_METADATA_PROFILE_COMPATIBILITY_FLAGS_PADDING, 0x00);
    metadata.insert(metadata.end(), H265_METADATA_CONSTRAINT_INDICATOR_FLAGS);
    metadata.insert(metadata.end(), H265_METADATA_CONSTRAINT_INDICATOR_FLAGS_PADDING, 0x00);
    metadata.insert(metadata.end(), H265_METADATA_GENERAL_LEVEL_IDC);

    metadata.insert(metadata.end(), (H265_METADATA_MIN_SPATIAL_SEGMENTATION >> 8) | H265_METADATA_MIN_SPATIAL_SEGMENTATION_RESERVED_BYTES);
    metadata.insert(metadata.end(), H265_METADATA_MIN_SPATIAL_SEGMENTATION);
    metadata.insert(metadata.end(), H265_METADATA_PARALLELISM_TYPE_RESERVED_BYTES | H265_METADATA_PARALLELISM_TYPE);
    metadata.insert(metadata.end(), H265_METADATA_CHROMA_FORMAT_RESERVED_BYTES | H265_METADATA_CHROMA_FORMAT);
    metadata.insert(metadata.end(), H265_METADATA_BIT_DEPTH_LUMA_MINUS_8_RESERVED_BYTES | H265_METADATA_BIT_DEPTH_LUMA_MINUS_8);
    metadata.insert(metadata.end(), H265_METADATA_BIT_DEPTH_CHROMA_MINUS_8_RESERVED_BYTES | H265_METADATA_BIT_DEPTH_CHROMA_MINUS_8);
    metadata.insert(metadata.end(), H265_METADATA_AVG_FRAMERATE >> 8);
    metadata.insert(metadata.end(), H265_METADATA_AVG_FRAMERATE);
    metadata.insert(metadata.end(), 
        (H265_METADATA_CTX_FRAMERATE << 6) | 
        (H265_METADATA_NUM_TEMPORAL_LAYERS << 3) |
        (H265_METADATA_TEMPORAL_ID_NESTED << 2) |
        (H265_METADATA_LENGTH_SIZE_MINUS_ONE)
    );
    metadata.insert(metadata.end(), H265_NUMBER_OF_ARRAYS);

    metadata.insert(metadata.end(), VPS);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY >> 8);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY);
    metadata.insert(metadata.end(), (vpsLength >> 8) & 0xFF);
    metadata.insert(metadata.end(), vpsLength & 0xFF);
    metadata.insert(metadata.end(), vps.begin(), vps.end());

    metadata.insert(metadata.end(), SPS_HEVC);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY >> 8);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY);
    metadata.insert(metadata.end(), (spsLength >> 8) & 0xFF);
    metadata.insert(metadata.end(), spsLength & 0xFF);
    metadata.insert(metadata.end(), sps.begin(), sps.end());

    metadata.insert(metadata.end(), PPS_HEVC);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY >> 8);
    metadata.insert(metadata.end(), H265_NUM_NALUS_IN_ARRAY);
    metadata.insert(metadata.end(), (ppsLength >> 8) & 0xFF);
    metadata.insert(metadata.end(), ppsLength & 0xFF);
    metadata.insert(metadata.end(), pps.begin(), pps.end());
}