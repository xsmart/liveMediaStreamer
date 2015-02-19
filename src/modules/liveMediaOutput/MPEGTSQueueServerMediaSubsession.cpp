/*
 *  MPEGTSQueueServerMediaSubsession.cpp - A subsession class for MPEGTS
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
 *  Authors:  David Cassany <david.cassany@i2cat.net>,
 *            
 */

#include "MPEGTSQueueServerMediaSubsession.hh"
#include "H264StartCodeInjector.hh"
#include "../../Utils.hh"

MPEGTSQueueServerMediaSubsession::MPEGTSQueueServerMediaSubsession(
    UsageEnvironment& env, Boolean reuseFirstSource) : 
    QueueServerMediaSubsession(env, reuseFirstSource), aReaderId(-1), vReaderId(-1)
{
}

MPEGTSQueueServerMediaSubsession* MPEGTSQueueServerMediaSubsession::createNew(
    UsageEnvironment& env, Boolean reuseFirstSource)
{
    return new MPEGTSQueueServerMediaSubsession(env, reuseFirstSource);
}

MPEGTSQueueServerMediaSubsession::~MPEGTSQueueServerMediaSubsession()
{
}

std::vector<int> MPEGTSQueueServerMediaSubsession::getReaderIds()
{
    std::vector<int> readers;
    if (aReaderId != -1){
        readers.push_back(aReaderId);
    }
    
    if (vReaderId != -1){
        readers.push_back(vReaderId);
    }
    
    return readers;
}

 
bool MPEGTSQueueServerMediaSubsession::addVideoSource(VCodecType codec, StreamReplicator* replicator, int readerId)
{  
    if (codec != H264) {
        utils::errorMsg("Error creating MPEG-TS Connection. Only H264 video codec is valid");
        return false;
    }

    if (!replicator) {
        utils::errorMsg("Error adding video source to MPEG-TS Connection. Provided source is NULL");
        return false;
    }

    if (vReaderId == -1){
        vReaderId = readerId;
    } else {
        utils::errorMsg("Error video reader ID was already set.");
        return false;
    }
    
    
    vReplicator = replicator;
    
    return true;
}
    
bool MPEGTSQueueServerMediaSubsession::addAudioSource(ACodecType codec, StreamReplicator* replicator,
                                                      int readerId)
{
    if (codec != AAC) {
        utils::errorMsg("Error creating MPEG-TS Connection. Only AAC audio codec is valid");
        return false;
    }
    
    if (!replicator) {
        utils::errorMsg("Error adding video source to MPEG-TS Connection. Provided source is NULL");
        return false;
    }
    
    if (aReaderId == -1){
        aReaderId = readerId;
    } else {
        utils::errorMsg("Error video reader ID was already set.");
        return false;
    }

    aReplicator = replicator;
    
    return true;
}



FramedSource* MPEGTSQueueServerMediaSubsession::createNewStreamSource(unsigned clientSessionId,
                                                                      unsigned& estBitrate)
{
    FramedSource* startCodeInjector;
    MPEG2TransportStreamFromESSource* tsFramer;
    //TODO: WTF
    estBitrate = 2000; // kbps, estimate
    
    tsFramer = MPEG2TransportStreamFromESSource::createNew(envir());
    
    if (vReplicator){
        startCodeInjector = H264StartCodeInjector::createNew(envir(), vReplicator->createStreamReplica());
        tsFramer->addNewVideoSource(startCodeInjector, 5/*mpegVersion: H.264*/);
    }
    
    if (aReplicator){
        tsFramer->addNewVideoSource(aReplicator->createStreamReplica(), 4/*mpegVersion: AAC*/);
    }
       
    return tsFramer;
}

RTPSink* MPEGTSQueueServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
                                                            unsigned char rtpPayloadTypeIfDynamic,
                                                            FramedSource* inputSource)
{
    return SimpleRTPSink::createNew(envir(), rtpGroupsock, 33, 90000, "video", 
                                     "MP2T", 1, True, False /*no 'M' bit*/);
}

