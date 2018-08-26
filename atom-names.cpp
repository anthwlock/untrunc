//==================================================================//
/*                                                                  *
	Untrunc - atom-names.cpp

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.

	Untrunc is distributed under the GPL "AS IS", without
	any warranty; without the implied warranty of merchantability
	or fitness for either an expressed or implied particular purpose.

	Please see the included GNU General Public License (GPL) for
	your rights and further details; see the file COPYING. If you
	cannot, write to the Free Software Foundation, 59 Temple Place
	Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

	Copyright 2010 Federico Ponchio
	with contributions from others.
 *                                                                  */
//==================================================================//

#include "atom-names.h"

#include "common.h"


// Full atom name mappings.
// Some atom codes/tags have multiple meanings depending on their path.
const std::map<uint32_t, const char*> AtomFullNames = {
	//
	// MP4RA Atoms/Boxes
	//

	// ISO family codes.
	{name2Id("ainf"), "Asset Information"},  // Used to identify, license and play.
	{name2Id("assp"), "Alternative Startup Sequence Properties"},
	{name2Id("avcn"), "AVC NAL Unit storage"},
	{name2Id("bloc"), "Base Location"},  // Purchase location for license acquisition.
	{name2Id("bpcc"), "Bits Per Component"},
	{name2Id("buff"), "Buffering information"},
	{name2Id("bxml"), "Binary XML container"},
	{name2Id("ccid"), "OMA DRM Content ID"},
	{name2Id("cdef"), "Component Definition"},  // Type and ordering of components within the codestream.
	{name2Id("cmap"), "Component to palette Mapping"},  // Mapping between a palette and codestream Components.
	{name2Id("co64"), "64-bit Chunk Offset"},
	{name2Id("coin"), "Content Information"},
	//{name2Id("colr"), "Colour Specification"},  // Colourspace of the image
	{name2Id("crhd"), "Clock Reference stream Header"},
	{name2Id("cslg"), "Composition to decode timeline mapping"},
	{name2Id("ctts"), "Composition Time To Sample"},
	{name2Id("cvru"), "OMA DRM Cover URI"},
	{name2Id("dinf"), "Data Information container"},
	{name2Id("dref"), "Data Reference"},  // Declares source(s) of media data in track.
	{name2Id("dsgd"), "DVB Sample Group Description"},
	{name2Id("dstg"), "DVB Sample To Group"},
	{name2Id("edts"), "Edit list container"},
	{name2Id("elst"), "Edit List"},
	{name2Id("emsg"), "Event Message"},
	{name2Id("evti"), "Event Information"},
	{name2Id("fdel"), "File Delivery information"}, // Item info extension.
	{name2Id("feci"), "FEC Informatiom"},  // Forward Error Correction.
	{name2Id("fecr"), "FEC Reservoir"},
	{name2Id("fiin"), "File Descriptor Item Information"},
	{name2Id("fire"), "File Reservoir"},
	{name2Id("fpar"), "File Partition"},
	{name2Id("free"), "Free space"},
	{name2Id("frma"), "Original Format"},
	{name2Id("ftyp"), "File Type"},  // File Type and compatibility (contains list of MP4RA Brands).
	{name2Id("gitn"), "Group ID To Name"},
	{name2Id("grpi"), "OMA DRM Group ID"},
	{name2Id("hdlr"), "Handler type"},  // Declares the media (handler) type.
	{name2Id("hmhd"), "Hint Media Header"},  // Overall track information.
	{name2Id("hpix"), "Hipix Rich Picture"},  // User-data or Meta-data.
	{name2Id("icnu"), "OMA DRM Icon URI"},
	{name2Id("ID32"), "ID3 version 2 container"},
	{name2Id("idat"), "Item Data"},
	{name2Id("ihdr"), "Image Header"},
	{name2Id("iinf"), "Item Information"},
	{name2Id("iloc"), "Item (data) Location"},
	{name2Id("imif"), "IPMP Information"},
	{name2Id("infe"), "Item Information Entry"},
	{name2Id("infu"), "OMA DRM Info URL"},
	{name2Id("iods"), "Object Descriptor container"},
	{name2Id("iphd"), "IPMP stream Header"},
	{name2Id("ipmc"), "IPMP Control"},
	{name2Id("ipro"), "Item Protection"},
	{name2Id("iref"), "Item Reference"},
	{name2Id("jP  "), "JPEG 2000 signature"},
	{name2Id("jp2c"), "JPEG 2000 contiguous Codestream"},
	{name2Id("jp2h"), "JPEG 2000 Header"},
	{name2Id("jp2i"), "JPEG 2000 Intellectual property information"},
	{name2Id("leva"), "Leval Assignment"},
	{name2Id("loop"), "Looping behavior"},
	{name2Id("lrcu"), "OMA DRM Lyrics URI"},
	{name2Id("m7hd"), "MPEG-7 stream Header"},
	{name2Id("mdat"), "Media Data container"},
	{name2Id("mdhd"), "Media Header"},  // Overall information about the media
	{name2Id("mdia"), "Media information container"},  // Container for the Media information in a track.
	{name2Id("mdri"), "Mutable DRM Information"},
	{name2Id("meco"), "Additional Metadata Container"},
	{name2Id("mehd"), "Movie Extends Header"},
	{name2Id("mere"), "Metabox Relation"},
	{name2Id("meta"), "Meta-data container"},
	{name2Id("mfhd"), "Movie Fragment Header"},
	{name2Id("mfra"), "Movie Fragment Random Access"},
	{name2Id("mfro"), "Movie Fragment Random access Offset"},
	{name2Id("minf"), "Media Information container"},
	{name2Id("mjhd"), "MPEG-J stream Header"},
	{name2Id("moof"), "Movie Fragment"},
	{name2Id("moov"), "Container for all the Meta-data"},
	{name2Id("mvcg"), "Multiview Group"},
	{name2Id("mvci"), "Multiview Information"},
	{name2Id("mvex"), "Movie Extends"},
	{name2Id("mvhd"), "Movie Header"},  // Overall declarations.
	{name2Id("mvra"), "Multiview Relation Attribute"},
	{name2Id("nmhd"), "Null Media Header"},  // Overall information (some tracks only).
	{name2Id("ochd"), "Object Content info stream Header"},
	{name2Id("odaf"), "OMA DRM Access unit Format"},
	{name2Id("odda"), "OMA DRM content object"},
	{name2Id("odhd"), "Object Descriptor stream Header"},
	{name2Id("odhe"), "OMA DRM discrete media Headers"},
	{name2Id("odrb"), "OMA DRM Rights Object"},
	{name2Id("odrm"), "OMA DRM container"},
	{name2Id("odtt"), "OMA DRM Transaction Tracking"},
	{name2Id("ohdr"), "OMA DRM common Headers"},
	{name2Id("padb"), "Sample Padding Bits"},
	{name2Id("paen"), "Partition Entry"},
	{name2Id("pclr"), "Palette mapping of components"},  // Palette which maps a single component in index space to a multiple-component image.
	{name2Id("pdin"), "Progressive Download Information"},
	{name2Id("pitm"), "Primary Item reference"},
	{name2Id("prft"), "Producer Reference Time"},
	{name2Id("pssh"), "Protection System Specific Header"},
	{name2Id("res "), "Grid Resolution"},
	{name2Id("resc"), "Captured Grid Resolution"},  // Grid Resolution at which the image was Captured.
	{name2Id("resd"), "Default Grid Resolution"},  // Default grid Resolution at which the image should be Displayed.
	{name2Id("rinf"), "Restricted scheme Information"},
	{name2Id("saio"), "Sample Auxiliary Information Offsets"},
	{name2Id("saiz"), "Sample Auxiliary Information Sizes"},
	{name2Id("sbgp"), "Sample to Group"},
	{name2Id("schi"), "Scheme Information"},
	{name2Id("schm"), "Scheme type"},
	{name2Id("sdep"), "Sample Dependency"},
	{name2Id("sdhd"), "Scene Description stream Header"},
	{name2Id("sdtp"), "Independent and Disposable Samples"},
	{name2Id("sdvp"), "SD card Profile"},
	{name2Id("segr"), "File delivery Session Group"},
	{name2Id("senc"), "Sample specific Encryption data"},
	{name2Id("sgpd"), "Sample Group Definition"},
	{name2Id("sidx"), "Segment Index"},
	{name2Id("sinf"), "Protection Scheme Information"},
	{name2Id("skip"), "Skip obsolete atom (free space)"},
	{name2Id("smhd"), "Sound Media Header"},  // Overall information (sound track only).
	{name2Id("srmb"), "System Renewability Message"},
	{name2Id("srmc"), "System Renewability Message Container"},
	{name2Id("srpp"), "STRP Process"},
	{name2Id("ssix"), "Sub-Sample Index"},
	{name2Id("stbl"), "Sample Table"},  // Container for the time/space map.
	{name2Id("stco"), "Chunk Offset"},  // Partial data-offset information.
	{name2Id("stdp"), "Sample Degradation Priority"},
	{name2Id("sthd"), "Subtitle media Header"},
	{name2Id("strd"), "Sub-track Definition"},
	{name2Id("stri"), "Sub-track Information"},
	{name2Id("stsc"), "Sample-To-Chunk"},  // Partial data-offset information.
	{name2Id("stsd"), "Sample Descriptions"},  // Codec types, initialization etc.
	{name2Id("stsg"), "Sub-track Sample Grouping"},
	{name2Id("stsh"), "Shadow sync Sample Table"},
	{name2Id("stss"), "Sync Sample Table (random access points)"},
	{name2Id("stsz"), "Sample Sizes"},  // Framing
	{name2Id("stts"), "Time-To-Sample"},  // Decoding.
	{name2Id("styp"), "Segment Type"},
	{name2Id("stz2"), "Compact Sample Sizes"},  // Framing.
	{name2Id("subs"), "Sub-sample information"},
	{name2Id("swtc"), "Multiview Group Relation"},
	{name2Id("tfad"), "Track Fragment Adjustment"},
	{name2Id("tfdt"), "Track Fragment Decode Time"},
	{name2Id("tfhd"), "Track Fragment Header"},
	{name2Id("tfma"), "Track Fragment Media Adjustment"},
	{name2Id("tfra"), "Track Fragment Radom Access"},
	{name2Id("tibr"), "Tier Bit Rate"},
	{name2Id("tiri"), "Tier Information"},
	{name2Id("tkhd"), "Track Header"},  // Overall information about the track.
	{name2Id("traf"), "Track Fragment"},
	{name2Id("trak"), "Track container"},  // Container for an individual track or stream.
	{name2Id("tref"), "Track Reference container"},
	{name2Id("trep"), "Track Extension Properties"},
	{name2Id("trex"), "Track Extends defaults"},
	{name2Id("trgr"), "Track Grouping information"},
	{name2Id("trik"), "Trick play modes"},  // Facilitates random access and trick play modes.
	{name2Id("trun"), "Track fragment Run"},
	{name2Id("udta"), "User-Data"},
	{name2Id("uinf"), "UUID Information"},  // A tool by which a vendor may provide access to additional information associated with a UUID.
	{name2Id("UITS"), "Unique Identifier Technology Solution"},
	{name2Id("ulst"), "UUID List"},
	//{name2Id("url "), "URL"},
	{name2Id("uuid"), "UUID user extension"},
	{name2Id("vmhd"), "Video Media Header"},  // Overall information (video track only).
	{name2Id("vwdi"), "Multiview scene Information"},
	{name2Id("xml "), "XML container"},  // A tool by which vendors can add XML formatted information.

	// User-Data codes.
	{name2Id("albm"), "Album title and track number for media"},
	{name2Id("alou"), "Album Loudness base"},
	{name2Id("angl"), "Name of the Camara Angle"},  // Name of the camera angle through which the clip was shot.
	{name2Id("auth"), "Author of the media"},
	{name2Id("clfn"), "Name of the Clip File"},
	{name2Id("clid"), "Identifier of the Clip"},
	{name2Id("clsf"), "Classification of the media"},
	{name2Id("cmid"), "Identifier of the Camera"},
	{name2Id("cmnm"), "Name that identifies the Camera"},
	{name2Id("coll"), "Collection name from which the media comes"},
	{name2Id("cprt"), "Copyright etc."},
	{name2Id("date"), "Date and time"},  // Formatted according to ISO 8601, when the content was created. For clips captured by recording devices, this is typically the date and time when the clip’s recording started.
	{name2Id("dscp"), "Media Description"},
	{name2Id("gnre"), "Media Genre"},
	{name2Id("hinf"), "Hint Information"},
	{name2Id("hnti"), "Hint information"},
	{name2Id("kywd"), "Media Keywords"},
	{name2Id("loci"), "Media Location Information"},
	{name2Id("ludt"), "Track Loudness container"},
	{name2Id("manu"), "Manufacturer name of the camera"},
	{name2Id("modl"), "Model name of the camera"},
	{name2Id("orie"), "Orientation information"},
	{name2Id("perf"), "Media Performer name"},
	{name2Id("reel"), "Name of the tape Reel"},
	{name2Id("rtng"), "Media Rating"},
	{name2Id("scen"), "Name of the Scene"},  // Name of the Scene for which the clip was shot.
	{name2Id("shot"), "Name that identifies the shot"},
	{name2Id("slno"), "Serial number of the camera"},
	{name2Id("strk"), "Sub-track information"},
	{name2Id("thmb"), "Thumbnail image of the media"},
	{name2Id("titl"), "Media Title"},
	{name2Id("tlou"), "Track Loudness base"},
	{name2Id("tsel"), "Track Selection"},
	{name2Id("urat"), "User 'star' Rating of the media"},
	{name2Id("yrrc"), "Year when media was Recorded"},

	// Quick Time codes.
	{name2Id("clip"), "Visual Clipping region container"},
	{name2Id("crgn"), "Visual Clipping Region definition"},
	{name2Id("ctab"), "Track Colour-Table"},
	{name2Id("dcfD"), "Marlin DCF Duration"},  // User-data.
	{name2Id("elng"), "Extended Language"},
	{name2Id("imap"), "Track Input Map definition"},
	{name2Id("kmat"), "Compressed visual track Matte"},
	{name2Id("load"), "Track pre-Load definitions"},
	{name2Id("matt"), "Visual track Matte for compositing"},
	{name2Id("pnot"), "Preview Container"},
	{name2Id("wide"), "Expansion space reservation (free space)"},


	//
	// MP4RA Codecs
	//

	// Sample Entry Codes.
	{name2Id("2dcc"), "2D Cartesian Coordinates"},
	{name2Id("3glo"), "3GPP Location"},
	{name2Id("3gor"), "3GPP Orientation"},
	{name2Id("3gvo"), "3GPP Video Orientation"},
	{name2Id("a3ds"), "Auro-Cx 3D audio"},
	{name2Id("ac-3"), "AC-3 audio"},
	{name2Id("ac-4"), "AC-4 audio"},
	{name2Id("ac-5"), "AC-5 audio"},
	{name2Id("ac-6"), "AC-6 audio"},
	{name2Id("ac-7"), "AC-7 audio"},
	{name2Id("ac-8"), "AC-8 audio"},
	{name2Id("ac-9"), "AC-9 audio"},
	{name2Id("alac"), "Apple Lossless Audio Codec"},
	{name2Id("alaw"), "a-Law"},
	{name2Id("avc1"), "Advanced Video Coding v1"},
	{name2Id("avc2"), "Advanced Video Coding v2"},
	{name2Id("avc3"), "Advanced Video Coding v3"},
	{name2Id("avc4"), "Advanced Video Coding v4"},
	{name2Id("avc5"), "Advanced Video Coding v5"},
	{name2Id("avc6"), "Advanced Video Coding v6"},
	{name2Id("avc7"), "Advanced Video Coding v7"},
	{name2Id("avc8"), "Advanced Video Coding v8"},
	{name2Id("avc9"), "Advanced Video Coding v9"},
	{name2Id("avcp"), "Advanced Video Coding Parameters"},
	{name2Id("dra1"), "DRA Audio"},
	{name2Id("drac"), "Dirac video coder"},
	{name2Id("dts+"), "Enhancement layer for DTS layered audio"},
	{name2Id("dts-"), "Dependent base layer for DTS layered audio"},
	{name2Id("dtsc"), "Core Substream"},
	{name2Id("dtse"), "Extension Substream containing only LBR"},
	{name2Id("dtsh"), "Core Substream + extension substream"},
	{name2Id("dtsl"), "Extension Substream containing only XLL"},
	{name2Id("dtsx"), "DTS-UHD profile 2"},
	{name2Id("dtsy"), "DTS-UHD profile 3 or higher"},
	{name2Id("dva1"), "AVC-based Dolby Vision derived from avc1"},
	{name2Id("dvav"), "AVC-based Dolby Vision derived from avc3"},
	{name2Id("dvh1"), "HEVC-based Dolby Vision derived from hvc1"},
	{name2Id("dvhe"), "HEVC-based Dolby Vision derived from hev1"},
	{name2Id("ec-3"), "Enhanced AC-3 audio"},
	{name2Id("ec+3"), "Enhanced AC-3 audio with JOC"},
	{name2Id("enca"), "Encrypted/Protected Audio"},
	{name2Id("encf"), "Encrypted/Protected Font"},
	{name2Id("encm"), "Encrypted/Protected Meta-data"},
	{name2Id("encs"), "Encrypted Systems stream"},
	{name2Id("enct"), "Encrypted Text"},
	{name2Id("encv"), "Encrypted/protected Video"},
	{name2Id("fdp "), "File Delivery hints"},
	{name2Id("g719"), "ITU G.719"},  // ITU-T Recommendation G.719 (2008).
	{name2Id("g726"), "ITU G.726"},  // ITU-T Recommendation G.726 (1990).
	{name2Id("hev1"), "High Efficiency Video coding v1"},
	{name2Id("hvc1"), "High Efficiency Video Coding v1"},
	{name2Id("ixse"), "DVB track level Index track"},
	{name2Id("m2ts"), "MPEG-2 Transport Stream for DMB"},
	{name2Id("m4ae"), "MPEG-4 Audio Enhancement"},
	{name2Id("mett"), "Text Timed Meta-data that is not XML"},
	{name2Id("metx"), "XML Timed Meta-data"},
	{name2Id("mha1"), "MPEG-H Audio (single stream"},
	{name2Id("mha2"), "MPEG-H Audio (multi-stream"},
	{name2Id("mhm1"), "MPEG-H Audio (single stream"},
	{name2Id("mhm2"), "MPEG-H Audio (multi-stream"},
	{name2Id("mjp2"), "Motion JPEG 2000"},
	{name2Id("mlix"), "DVB Movie level Index track"},
	{name2Id("mlpa"), "MLP Audio"},
	{name2Id("mp4a"), "MPEG-4 Audio"},
	{name2Id("mp4s"), "MPEG-4 Systems"},
	{name2Id("mp4v"), "MPEG-4 Visual"},
	{name2Id("mvc1"), "Multiview Coding v1"},
	{name2Id("mvc2"), "Multiview Coding v2"},
	{name2Id("mvc3"), "Multiview Coding v3"},
	{name2Id("mvc4"), "Multiview Coding v4"},
	{name2Id("mvc5"), "Multiview Coding v5"},
	{name2Id("mvc6"), "Multiview Coding v6"},
	{name2Id("mvc7"), "Multiview Coding v7"},
	{name2Id("mvc8"), "Multiview Coding v8"},
	{name2Id("mvc9"), "Multiview Coding v9"},
	{name2Id("oksd"), "OMA Keys"},
	{name2Id("Opus"), "Opus audio coding"},
	{name2Id("pm2t"), "Protected MPEG-2 Transport"},
	{name2Id("prtp"), "Protected RTP reception"},
	{name2Id("raw "), "Uncompressed audio"},
	{name2Id("resv"), "Restricted Video"},
	{name2Id("rm2t"), "MPEG-2 Transport Reception"},
	{name2Id("rrtp"), "RTP reception"},
	{name2Id("rsrp"), "SRTP Reception"},
	{name2Id("rtmd"), "Real Time Meta-data sample entry (XAVC format)"},
	{name2Id("rtp "), "RTP hints"},
	{name2Id("rv60"), "RealVideo codec 11"},
	{name2Id("s263"), "ITU H.263 video (3GPP format)"},
	{name2Id("samr"), "Narrowband AMR voice"},
	{name2Id("sawb"), "Wideband AMR voice"},
	{name2Id("sawp"), "Extended AMR-WB (AMR-WB+)"},
	{name2Id("sevc"), "EVRC voice"},
	{name2Id("sevs"), "Enhanced Voice Services (EVS)"},
	{name2Id("sm2t"), "MPEG-2 Transport server"},
	{name2Id("sqcp"), "13K voice"},
	{name2Id("srtp"), "SRTP hints"},
	{name2Id("ssmv"), "SMV voice"},
	{name2Id("STGS"), "Subtitle sample entry (HMMP)"},
	{name2Id("stpp"), "Subtitles (timed text)"},
	{name2Id("svc1"), "Scalable Video Coding v1"},
	{name2Id("svc2"), "Scalable Video Coding v2"},
	{name2Id("svc3"), "Scalable Video Coding v3"},
	{name2Id("svc4"), "Scalable Video Coding v4"},
	{name2Id("svc5"), "Scalable Video Coding v5"},
	{name2Id("svc6"), "Scalable Video Coding v6"},
	{name2Id("svc7"), "Scalable Video Coding v7"},
	{name2Id("svc8"), "Scalable Video Coding v8"},
	{name2Id("svc9"), "Scalable Video Coding v9"},
	{name2Id("svcM"), "SVC Meta-data"},
	{name2Id("tc64"), "64-bit Timecode samples"},
	//{name2Id("tmcd"), "32-bit Timecode samples"},
	{name2Id("twos"), "Uncompressed 16-bit PCM audio (BE)"},  // Signed 16-bit big-endian PCM audio (see: sowt).
	{name2Id("tx3g"), "Timed Text stream"},
	{name2Id("ulaw"), "mu-Law 2:1"},
	{name2Id("unid"), "Dynamic Range Control (DRC) data"},
	{name2Id("urim"), "Binary timed Meta-data identified by URI"},
	{name2Id("vc-1"), "SMPTE VC-1"},
	{name2Id("vc-2"), "SMPTE VC-2"},
	{name2Id("vc-3"), "SMPTE VC-3"},
	{name2Id("vp08"), "VP8 video"},
	{name2Id("vp09"), "VP9 video"},
	{name2Id("vp10"), "VP10 video"},
	{name2Id("vp11"), "VP11 video"},
	{name2Id("vp12"), "VP12 video"},
	{name2Id("vp13"), "VP13 video"},
	{name2Id("vp14"), "VP14 video"},
	{name2Id("vp15"), "VP15 video"},
	{name2Id("vp16"), "VP16 video"},
	{name2Id("vp17"), "VP17 video"},
	{name2Id("vp18"), "VP18 video"},
	{name2Id("vp19"), "VP19 video"},
	{name2Id("wvtt"), "WebVTT data"},

	// Meta-data Item Type Codes.
	{name2Id("auvd"), "Auxiliary Video Descriptor"},

	// Box types contained in specific Sample Entries.
	{name2Id("avcC"), "AVC Configuration"},
	{name2Id("btrt"), "Bit-rate information"},
	{name2Id("chnl"), "Channel Layout"},
	{name2Id("clap"), "Clean Aperture"},
	{name2Id("colr"), "Colour information"},  // See the color information in the 'misc' tables for the contents of the color information box.
	{name2Id("CoLL"), "Content Light Level"},
	{name2Id("dac3"), "Decoder specific info for AC-3 audio"},
	{name2Id("dac4"), "Decoder specific info for AC-4 audio"},
	{name2Id("dac5"), "Decoder specific info for AC-5 audio"},
	{name2Id("dac6"), "Decoder specific info for AC-6 audio"},
	{name2Id("dac7"), "Decoder specific info for AC-7 audio"},
	{name2Id("dac8"), "Decoder specific info for AC-8 audio"},
	{name2Id("dac9"), "Decoder specific info for AC-9 audio"},
	{name2Id("dec3"), "Decoder specific info for Enhanced AC-3 audio"},
	{name2Id("dmix"), "Downmix instructions"},
	{name2Id("dmlp"), "Decoder specific info for MLP audio"},
	{name2Id("dvcC"), "Dolby Vision Configuration"},
	{name2Id("dvvC"), "Dolby Vision Extended Configuration"},
	//{name2Id("ecam"), "Extrinsic Camera parameters"},
	{name2Id("esds"), "Elementary Stream Descriptor"},
	{name2Id("hvcC"), "HEVC Configuration"},
	//{name2Id("icam"), "Intrinsic Camera parameters"},
	{name2Id("leqi"), "Loudness Equalization Instructions"},
	{name2Id("m4ds"), "MPEG-4 Descriptors"},
	{name2Id("mvcC"), "MVC Configuration"},
	{name2Id("mvcP"), "MVC Priority assignment"},
	{name2Id("pasp"), "Pixel Aspect ratio"},
	{name2Id("pdc1"), "Parametric DRC Coefficients"},
	{name2Id("pdi1"), "Parametric DRC Instructions"},
	{name2Id("qlif"), "Layer Quality assignments"},
	{name2Id("seib"), "Scalability Information"},
	{name2Id("SmDm"), "Mastering Display Meta-data"},
	{name2Id("svcC"), "SVC Configuration"},
	{name2Id("svcP"), "SVC Priority assignments"},
	{name2Id("svmC"), "SVC Information Configuration"},
	{name2Id("udc1"), "Basic DRC Coefficients"},
	{name2Id("udc2"), "Unified DRC Coefficients"},
	{name2Id("udex"), "Unified DRC configuration Extension"},
	{name2Id("udi1"), "Basic DRC Instructions"},
	{name2Id("udi2"), "Unified DRC Instructions"},
	{name2Id("ueqc"), "Equalization Coefficients"},
	{name2Id("ueqi"), "Equalization Instructions"},
	{name2Id("uriI"), "URI Information"},
	{name2Id("vpcC"), "VP9 Codec Configuration"},
	{name2Id("vsib"), "View scalability Information"},
	{name2Id("vwid"), "View Identifier"},

	// QuickTime Sample Entry Codes.
	{name2Id("agsm"), "GSM"},
	//{name2Id("alaw"), "a-Law"},
	{name2Id("CFHD"), "CineForm High-Definition (HD) wavelet codec"},
	{name2Id("civd"), "Cinepak Video"},
	{name2Id("c608"), "CEA 608 captions"},
	{name2Id("c708"), "CEA 708 captions"},
	//{name2Id("drac"), "Dirac video coder"},
	{name2Id("DV10"), "Digital Voodoo 10-bit uncompressed 4:2:2 codec"},
	{name2Id("dvh5"), "DVCPRO-HD 1080/50i"},
	{name2Id("dvh6"), "DVCPRO-HD 1080/60i"},
	{name2Id("dvhp"), "DVCPRO-HD 720/60p"},
	{name2Id("dvi "), "DVI (used in RTP, 4:1 compression)"},
	{name2Id("DVOO"), "Digital Voodoo 8-bit uncompressed 4:2:2 codec"},
	{name2Id("DVOR"), "Digital Voodoo intermediate raw"},
	{name2Id("DVTV"), "Digital Voodoo intermediate 2vuy"},
	{name2Id("DVVT"), "Digital Voodoo intermediate v210"},
	{name2Id("fl32"), "32-bit float"},
	{name2Id("fl64"), "64-bit float"},
	{name2Id("flic"), "Autodesk FLIC animation format"},
	{name2Id("gif"),  "GIF image format"},  // Graphics Interchange Format.
	{name2Id("gif "), "GIF image format"},
	{name2Id("h261"), "ITU H.261 video"},
	{name2Id("h263"), "ITU H.263 video (QuickTime format)"},
	{name2Id("HD10"), "Digital Voodoo 10-bit uncompressed 4:2:2 HD codec"},
	{name2Id("ima4"), "International Multimedia Assocation (IMA) 4:1"},
	{name2Id("in24"), "24-bit integer uncompressed"},
	{name2Id("in32"), "32-bit integer uncompressed"},
	{name2Id("jpeg"), "JPEG image format"},
	{name2Id("lpcm"), "Uncompressed Linear PCM audio (various integer and float formats)"},
	{name2Id("M105"), "Matrox hardware video data"},  // Internal format of video data supported by Matrox hardware; pixel organization is proprietary.
	{name2Id("mjpa"), "Motion-JPEG (format A)"},
	{name2Id("mjpb"), "Motion-JPEG (format B)"},
	//{name2Id("Opus"), "Opus audio coding"},
	{name2Id("png"),  "Portable Network Graphics (PNG)"},  // W3C, IETF RFC-2083, ISO 15948.
	{name2Id("png "), "Portable Network Graphics (PNG)"},  // W3C, IETF RFC-2083, ISO 15948.
	{name2Id("PNTG"), "Apple MacPaint image format"},
	{name2Id("Qclp"), "Qualcomm PureVoice"},
	{name2Id("QDM2"), "Qdesign Music 2"},
	{name2Id("QDMC"), "Qdesign Music 1"},
	{name2Id("rle "), "Apple animation codec"},
	{name2Id("rpza"), "Apple simple video 'Road Pizza' compression"},
	{name2Id("Shr0"), "Generic SheerVideo codec"},
	{name2Id("Shr1"), "SheerVideo RGB[A] 8b (8-bits/channel)"},
	{name2Id("Shr2"), "SheerVideo Y'CbCr[A] 8bv 4:4:4[:4] (8-bits/channel)"},
	{name2Id("Shr3"), "SheerVideo Y'CbCr 8bv 4:2:2 (2:1 chroma subsampling)"},
	{name2Id("Shr4"), "SheerVideo Y'CbCr 8bw 4:2:2 (2:1 chroma subsampling)"},
	{name2Id("SVQ1"), "Sorenson Video 1 video"},
	{name2Id("SVQ3"), "Sorenson Video 3 video"},
	{name2Id("tga"),  "Truvision Targa video format"},
	{name2Id("tga "), "Truvision Targa video format"},
	{name2Id("tiff"), "Tagged Image File Format (Adobe)"},
	//{name2Id("ulaw"), "mu-Law 2:1"},
	{name2Id("vdva"), "DV Audio (variable duration per video frame)"},
	{name2Id("WRLE"), "Windows BMP image format"},


	//
	// MP4RA Track references
	//

	// ISO Family code points.
	{name2Id("adda"), "Additional Audio track"},
	{name2Id("adrc"), "DRC meta-data track"},
	//{name2Id("avcp"), "AVC Parameter set stream link"},
	{name2Id("cdsc"), "Track Describing the referenced track"},
	{name2Id("dpnd"), "Track having an MPEG-4 Dependency on referenced track"},
	{name2Id("hind"), "Hint Dependency"},
	//{name2Id("hint"), "Links Hint track to original media track"},
	//{name2Id("iloc"), "Item data Location (item reference)"},
	{name2Id("ipir"), "Track containing IPI declarations for Referenced track"},
	{name2Id("lyra"), "Audio Layer track dependency"},
	{name2Id("mpod"), "OD track using referenced track as included elementary stream track"},
	{name2Id("sbas"), "Scalable Base"},
	{name2Id("scal"), "Scalable extraction"},
	{name2Id("swfr"), "AVC Switch From"},
	{name2Id("swto"), "AVC Switch To"},
	{name2Id("sync"), "Track using referenced track as Sync source"},
	{name2Id("tmcd"), "Time Code"},  // Usually references a time code track.
	{name2Id("vdep"), "Auxiliary Video Depth"},
	{name2Id("vplx"), "Auxiliary Video Parallax"},

	// QuickTime code-points.
	{name2Id("cdep"), "Structural Dependency"},
	{name2Id("chap"), "Chapter or scene list"},  // Usually references a text track.
	{name2Id("scpt"), "Transcript"},  // Usually references a text track.
	{name2Id("ssrc"), "Non-primary Source"},  // Indicates that the referenced track should send its data to this track.


	//
	// MP4RA Brands
	// Used inside ftyp atom.
	//
	{name2Id("3g2a"), "3GPP2"},
	{name2Id("3ge6"), "3GPP Release 6 extended-presentation Profile"},
	{name2Id("3ge7"), "3GPP Release 7 extended-presentation Profile"},
	{name2Id("3ge9"), "3GPP Release 9 Extended Presentation Profile"},
	{name2Id("3gf9"), "3GPP Release 9 File-delivery Server Profile"},
	{name2Id("3gg6"), "3GPP Release 6 General Profile"},
	{name2Id("3gg9"), "3GPP Release 9 General Profile"},
	{name2Id("3gh9"), "3GPP Release 9 Adaptive Streaming Profile"},
	{name2Id("3gm9"), "3GPP Release 9 Media Segment Profile"},
	{name2Id("3gp4"), "3GPP Release 4"},
	{name2Id("3gp5"), "3GPP Release 5"},
	{name2Id("3gp6"), "3GPP Release 6 basic Profile"},
	{name2Id("3gp7"), "3GPP Release 7"},
	{name2Id("3gp8"), "3GPP Release 8"},
	{name2Id("3gp9"), "3GPP Release 9 Basic Profile"},
	{name2Id("3gr6"), "3GPP Release 6 progressive-download Profile"},
	{name2Id("3gr9"), "3GPP Release 9 Progressive DownloadProfile"},
	{name2Id("3gs6"), "3GPP Release 6 streaming-server Profile"},
	{name2Id("3gs9"), "3GPP Release 9 Streaming ServerProfile"},
	{name2Id("3gt8"), "3GPP Release 8 Media Stream Recording Profile"},
	{name2Id("3gt9"), "3GPP Release 9 Media Stream Recording Profile"},
	{name2Id("ARRI"), "ARRI Digital Camera"},
	//{name2Id("avc1"), "Advanced Video Coding extensions"},
	{name2Id("bbxm"), "Blinkbox Master File (H.264 + 16-bit LPCM (LE))"},  // H.264 video and 16-bit little-endian LPCM audio.
	{name2Id("CAEP"), "Canon Digital Camera"},
	{name2Id("CDes"), "Convergent Designs"},
	{name2Id("caaa"), "CMAF Media Profile - AAC Adaptive Audio"},
	{name2Id("caac"), "CMAF Media Profile - AAC Core"},
	{name2Id("caqv"), "Casio Digital Camera"},
	{name2Id("ccea"), "CMAF Supplemental Data - CEA-608/708"},
	{name2Id("ccff"), "Common container file format"},
	{name2Id("cfhd"), "CMAF Media Profile - AVC HD"},
	{name2Id("cfsd"), "CMAF Media Profile - AVC SD"},
	{name2Id("chd1"), "CMAF Media Profile - HEVC HDR10"},
	{name2Id("chdf"), "CMAF Media Profile - AVC HDHF"},
	{name2Id("chhd"), "CMAF Media Profile - HEVC HHD8"},
	{name2Id("chh1"), "CMAF Media Profile - HEVC HHD10"},
	{name2Id("clg1"), "CMAF Media Profile - HEVC HLG10"},
	{name2Id("cmfc"), "CMAF Track Format"},
	{name2Id("cmff"), "CMAF Fragment Format"},
	{name2Id("cmfl"), "CMAF Chunk Format"},
	{name2Id("cmfs"), "CMAF Segment Format"},
	{name2Id("cud1"), "CMAF Media Profile - HEVC UHD10"},
	{name2Id("cud8"), "CMAF Media Profile - HEVC UHD8"},
	{name2Id("cwvt"), "CMAF Media Profile - WebVTT"},
	{name2Id("da0a"), "DMB AF audio with MPEG Layer II audio"},
	{name2Id("da0b"), "DMB AF"},
	{name2Id("da1a"), "DMB AF audio with ER-BSAC audio"},
	{name2Id("da1b"), "DMB AF"},
	{name2Id("da2a"), "DMB AF audio with HE-AAC v2 audio"},
	{name2Id("da2b"), "DMB AF audio with HE-AAC v2 audio and extensions"},  // DMB AF extending da2a.
	{name2Id("da3a"), "DMB AF audio with HE-AAC"},
	{name2Id("da3b"), "DMB AF audio with HE-AAC and BIFS"},  // AF extending da3a with BIFS.
	{name2Id("dash"), "ISOM DASH file"},  // ISO base media file format file specifically designed for DASH including movie fragments and Segment Index.
	{name2Id("dby1"), "MP4 files with Dolby content"},  // E.g. Dolby AC-4.
	{name2Id("dmb1"), "DMB AF supporting all components in spec"},  // DMB AF supporting all the components defined in the specification.
	{name2Id("dsms"), "ISOM DASH Self-Initializing Media Segment"},  // Media Segment conforming to the DASH Self-Initializing Media Segment format type for ISO base media file format"},
	{name2Id("dts1"), "MP4 track file with audio codecs dtsc dtsh or dtse"},
	{name2Id("dts2"), "MP4 track file with audio codec dtsx"},
	{name2Id("dts3"), "MP4 track file with audio codec dtsy"},
	{name2Id("dv1a"), "DMB AF video with AVC video"},
	{name2Id("dv1b"), "DMB AF"},
	{name2Id("dv2a"), "DMB AF video with AVC video"},
	{name2Id("dv2b"), "DMB AF video with AVC video and extensions"},  // DMB AF extending dv2a.
	{name2Id("dv3a"), "DMB AF video with AVC video"},
	{name2Id("dv3b"), "DMB AF video with AVC video and 3GPP timed text"},  // DMB AF extending dv3a with 3GPP timed text.
	{name2Id("dvr1"), "DVB RTP"},
	{name2Id("dvt1"), "DVB Transport Stream"},
	{name2Id("dxo "), "DxO ONE camera"},
	//{name2Id("emsg"), "Event message box present"},
	{name2Id("ifrm"), "Apple iFrame Specification"},
	{name2Id("im1i"), "CMAF Media Profile - IMSC1 Image"},
	{name2Id("im1t"), "CMAF Media Profile - IMSC1 Text"},
	{name2Id("isc2"), "File encrypted according to ISMACryp 2.0"},
	{name2Id("iso2"), "ISO file format version 2 (2004 edition)"},  // File based on the 2004 edition of the ISO file format,ISO
	{name2Id("iso3"), "ISO file format version 3"},
	{name2Id("iso4"), "ISO file format version 4"},
	{name2Id("iso5"), "ISO file format version 5"},
	{name2Id("iso6"), "ISO file format version 6"},
	{name2Id("iso7"), "ISO file format version 7"},
	{name2Id("iso8"), "ISO file format version 8"},
	{name2Id("iso9"), "ISO file format version 9"},
	{name2Id("isom"), "ISO Base Media File Format (ISOM)"},  // File based on the ISO Base Media File Format,ISO
	{name2Id("J2P0"), "JPEG 2000 Profile 0"},
	{name2Id("J2P1"), "JPEG 2000 Profile 1"},
	{name2Id("jp2 "), "JPEG 2000 Part 1"},
	{name2Id("jpm "), "JPEG 2000 Part 6 Compound Images"},
	{name2Id("jpsi"), "The JPSearch data interchange format"},
	{name2Id("jpx "), "JPEG 2000 Part 2"},
	{name2Id("jpxb"), "JPEG XR"},
	{name2Id("LCAG"), "Leica digital camera"},
	{name2Id("lmsg"), "ISOM Last Media Segment indicator"},  // For ISO base media file format.
	{name2Id("M4A "), "iTunes MPEG-4 audio protected or not"},
	{name2Id("M4B "), "iTunes AudioBook protected or not"},
	{name2Id("M4P "), "MPEG-4 protected audio"},  // AES
	{name2Id("M4V "), "MPEG-4 protected audio+video"},
	{name2Id("MFSM"), "Media File for Samsung video Meta-data"},
	{name2Id("MGSV"), "Home and Mobile Multimedia Platform (HMMP)"},
	{name2Id("mj2s"), "Motion JPEG 2000 simple profile"},
	//{name2Id("mjp2"), "Motion JPEG 2000"},
	//{name2Id("mp21"), "MPEG-21"},
	{name2Id("mp41"), "MP4 version 1"},
	{name2Id("mp42"), "MP4 version 2"},
	{name2Id("mp71"), "MPEG-7 file-level meta-data"},
	{name2Id("MPPI"), "Photo Player Multimedia Application Format"},
	{name2Id("mpuf"), "MMT Processing Unit Format"},
	{name2Id("msdh"), "ISOM general Media Segment"},  // Media Segment conforming to the general format type for ISO base media file format.
	{name2Id("msix"), "ISOM Indexed Media Segment"},  // Media Segment conforming to the Indexed Media Segment format type for ISO base media file format.
	{name2Id("MSNV"), "Portable multimedia CE product (MP4)"},  // Portable multimedia CE products using MP4 file format with AVC video codec and AAC audio codec.
	{name2Id("niko"), "Nikon Digital Camera"},
	{name2Id("odcf"), "OMA DCF (DRM Content Format)"},
	{name2Id("opf2"), "OMA PDCF (DRM Content Format)"},
	{name2Id("opx2"), "OMA Adapted PDCF"},
	{name2Id("pana"), "Panasonic Digital Camera"},
	{name2Id("piff"), "Protected Interoperable File Format"},
	{name2Id("pnvi"), "Panasonic Video Intercom"},
	{name2Id("qt  "), "QuickTime"},
	{name2Id("risx"), "MPEG-2 TS Representation Index Segment"},  // Used to index MPEG-2 TS based Media Segments.
	{name2Id("ROSS"), "Ross Video"},
	{name2Id("sdv "), "SD Video"},
	{name2Id("SEAU"), "Home and Mobile Multimedia Platform (HMMP)"},
	{name2Id("SEBK"), "Home and Mobile Multimedia Platform (HMMP)"},
	{name2Id("senv"), "Sony Entertainment Network provided Video (MP4)"},  // Video contents Sony Entertainment Network provides by using MP4 file format.
	{name2Id("sims"), "ISOM Sub-Indexed Media Segment"},  // Media Segment conforming to the Sub-Indexed Media Segment format type for ISO base media file format.
	{name2Id("sisx"), "MPEG-2 TS Single Index Segment"},  // Used to index MPEG-2 TS based Media Segments.
	{name2Id("ssss"), "MPEG-2 TS Sub-segment Index Segment"},  // Used to index MPEG-2 TS based Media Segments.
	{name2Id("uvvu"), "UltraViolet file brand"},  // Conforming to the DECE Common File Format spec.
	{name2Id("XAVC"), "XAVC file format"},


	//
	// MP4RA Miscellaneous
	//

	// Handlers.
	{name2Id("3gsd"), "3GPP Scene Description"},
	{name2Id("auxv"), "Auxiliary Video"},
	{name2Id("avmd"), "Avid Meta-Data"},
	{name2Id("clcp"), "Closed Caption"},
	{name2Id("cpad"), "CPCM Auxiliary meta-Data"},
	{name2Id("crsm"), "Clock Reference Stream"},
	{name2Id("dmbd"), "DVB Mandatory meta-data"},
	{name2Id("dtva"), "TV-Anytime meta-data"},
	{name2Id("dvmd"), "Dolby Vision Meta-Data"},
	{name2Id("fdsm"), "Font"},
	{name2Id("gesm"), "General MPEG-4 systems Streams"},  // Without specific handler.
	{name2Id("GRAP"), "Subtitle Graphics"},
	{name2Id("hint"), "Hint"},
	//{name2Id("hpix"), "Hipix Rich Picture Format"},
	//{name2Id("ID32"), "ID3 version 2 meta-data"},
	{name2Id("ipdc"), "DVB IPDC ESG meta-data"},
	{name2Id("ipsm"), "IPMP Stream"},
	{name2Id("m7sm"), "MPEG-7 Stream"},
	//{name2Id("meta"), "Meta-data"},
	{name2Id("mjsm"), "MPEG-J Stream"},
	{name2Id("mp21"), "MPEG-21 digital item"},
	{name2Id("mp7b"), "MPEG-7 Binary meta-data"},
	{name2Id("mp7t"), "MPEG-7 Textual meta-data"},
	{name2Id("MPEG"), "QuickTime MPEG track handler"},
	{name2Id("musi"), "QuickTime Music track handler"},
	{name2Id("nrtm"), "Non-Real Time Meta-data (XAVC format)"},
	{name2Id("null"), "No handling required (meta-data)"},
	{name2Id("ocsm"), "Object Content info Stream"},
	{name2Id("odsm"), "Object Descriptor Stream"},
	{name2Id("qd3d"), "QuickTime QuickDraw 3D track handler"},
	{name2Id("sbtl"), "QuickTime Subtitle track handler"},
	{name2Id("sdsm"), "Scene Description Stream"},
	{name2Id("skmm"), "Key Management Messages"},
	{name2Id("smhr"), "Samsung video Meta-data Handler"},
	{name2Id("soun"), "Audio"},  // Audio Media.
	{name2Id("sprt"), "QuickTime Sprite track handler"},
	{name2Id("strm"), "QuickTime Streaming track handler"},
	{name2Id("subt"), "Subtitles"},
	{name2Id("text"), "Text"},
	//{name2Id("tmcd"), "Time Code"},
	{name2Id("twen"), "QuickTime Tween track handler"},
	{name2Id("uri "), "URI identified meta-data"},
	{name2Id("vide"), "Video"},  // Visual Media.

	// Data References.
	{name2Id("url "), "URL data location"},
	{name2Id("urn "), "URN data location"},

	// Item References.
	//{name2Id("fdel"), "File Delivery reference"},
	//{name2Id("iloc"), "Item data Location"},

	// Multiview Attributes.
	{name2Id("bitr"), "Bit-rate"},
	{name2Id("ecam"), "Extrinsic Camera parameters"},
	{name2Id("elvi"), "Elliptical View array"},
	{name2Id("frar"), "Frame Rate"},
	{name2Id("geom"), "Geometry"},
	{name2Id("icam"), "Intrinsic Camera parameters"},
	{name2Id("ilvi"), "Inline View array"},
	{name2Id("levl"), "Level"},
	{name2Id("nvws"), "Number of output Views"},
	{name2Id("plvi"), "Planar View array"},
	{name2Id("prfl"), "Profile"},
	{name2Id("rtvi"), "Rectangular View array"},
	{name2Id("spvi"), "Spherical View array"},
	{name2Id("stvi"), "Stereo View array"},

	// Protection and Restricted Schemes.
	{name2Id("aSEI"), "SEI-restricted video"},
	{name2Id("cenc"), "Common Encryption"},
	{name2Id("cpcm"), "CPCM (DVB BlueBook A094) protected content"},
	{name2Id("odkm"), "OMA DRM KMS protection scheme"},
	//{name2Id("stvi"), "Stereo Video restricted scheme"},

	// Sample Groups.
	{name2Id("3gag"), "3GPP PSS Annex G video buffer parameters"},
	{name2Id("alst"), "Alternative Startup sequence"},
	{name2Id("avcb"), "AVC HRD parameters"},
	{name2Id("avll"), "AVC Layer description group"},
	{name2Id("avss"), "AVC Sub-sequence group"},
	{name2Id("dtrt"), "SVC Decode Re-timing"},
	{name2Id("mvif"), "MVC Scalability Information"},
	{name2Id("rap "), "Random Access Point"},
	{name2Id("rash"), "Rate Share"},
	{name2Id("roll"), "Pre/Post Roll group"},
	{name2Id("scif"), "SVC Scalability Information"},
	{name2Id("scnm"), "AVC/SVC/MVC Map groups"},
	{name2Id("stsa"), "Step-wise temporal layer"},
	//{name2Id("sync"), "Sync sample groups"},
	{name2Id("tele"), "Temporal Level"},
	{name2Id("tsas"), "Temporal Sub-layer"},
	{name2Id("tscl"), "Temporal Layer"},
	{name2Id("vipr"), "View Priority"},

	// Track Selection Types.
	//{name2Id("bitr"), "Track differs in Bit-rate"},
	{name2Id("bwas"), "Track differs by Bandwidth"},
	{name2Id("cdec"), "Track differs by Codec type in the Sample Entry"},
	{name2Id("cgsc"), "The track can be Coarse-grain Scaled."},
	{name2Id("fgsc"), "The track can be Fine-grain Scaled."},
	//{name2Id("frar"), "Track differs in Frame Rate"},
	{name2Id("lang"), "Track differs in Language"},
	{name2Id("mela"), "Track differs in Language"},
	{name2Id("mpsz"), "Track differs in the Maximum RTP Packet Size"},
	{name2Id("mtyp"), "Track differs in Media Type (handler type)"},
	//{name2Id("nvws"), "Number of Views in the sub track"},
	//{name2Id("resc"), "The track can be Region-of-interest Scaled."},
	{name2Id("scsz"), "Track differs in width and/or height"},
	{name2Id("spsc"), "The track can be Spatially Scaled."},
	{name2Id("tesc"), "The track can be Temporally Scaled."},
	{name2Id("vwsc"), "Sub-track can be Scaled in terms of number of Views"},

	// Track Group Types.
	{name2Id("msrc"), "Multi-Source presentation track grouping"},
	{name2Id("cstg"), "Complete Subset Track Grouping"},

	// Colour Types.
	{name2Id("nclx"), "Normal Colour information"},
	{name2Id("prof"), "Full ICC colour Profile"},
	{name2Id("rICC"), "Restricted ICC colour profile"},


	//
	// Other Tags/Codes (non-MP4RA)
	//

	// MP4 codes.
	{name2Id("alis"), "File Alias"},
	{name2Id("rsrc"), "Resource alias"},

	// Apple QuickTime codes.
	{name2Id("ACE2"), "Audio Compression/Expansion 2-to-1 (Apple IIGS)"},
	{name2Id("ACE8"), "Audio Compression/Expansion 8-to-3 (Apple IIGS)"},
	{name2Id("ADP4"), "Intel/DVI ADPCM 4:1"},
	{name2Id("DWVW"), "Delta with Variable word Width (TX16W Typhoon)"},
	{name2Id("MAC3"), "MACE 3-to-1 (Apple)"},
	{name2Id("MAC6"), "MACE 6-to-1 (Apple)"},
	{name2Id("QDMC"), "QDesign Music"},
	{name2Id("SDX2"), "Square Root Delta (BE) (3DO, Mac)"},  // Big-endian; 3DO (Panasonic) / MAC (Apple).
	{name2Id("rt24"), "RT24 50:1 (Voxware)"},
	{name2Id("rt29"), "RT29 50:1 (Voxware)"},
	{name2Id("sowt"), "Uncompressed 16-bit PCM audio (LE)"},  // Signed 16-bit little-endian PCM audio (see: twos).
	{name2Id("mhlr"), "Media Handler"},

	// iTunes codes.
	{name2Id("ilst"), "iTunes meta-data container"},
	{name2Id("data"), "iTunes meta-Data"},
	{name2Id("mdir"), "Meta-Data for iTunes Reader"},
	{name2Id("dhlr"), "Data Handler"},

	// Apple ProRes (used in Final Cut Pro).
	{name2Id("ap4c"), "Apple ProRes 4:4:4:4 (BE)"},  // 12-bit sample depth.
	{name2Id("c4pa"), "Apple ProRes 4:4:4:4 (LE)"},  // 12-bit sample depth. 
	{name2Id("ap4h"), "Apple ProRes 4:4:4:4 (BE)"},  // 12-bit sample depth. 
	{name2Id("h4pa"), "Apple ProRes 4:4:4:4 (LE)"},  // 12-bit sample depth.  
	{name2Id("ap4x"), "Apple ProRes 4:4:4:4 Extreme Quality (BE)"},  // 12-bit sample depth, for 4K UHD + HDR. 
	{name2Id("x4pa"), "Apple ProRes 4:4:4:4 Extreme Quality (LE)"},  // 12-bit sample depthm for 4K UHD + HDR.  
	{name2Id("apch"), "Apple ProRes 4:2:2 High Quality (BE)"},  // 10-bit sample depth.  
	{name2Id("hcpa"), "Apple ProRes 4:2:2 High Quality (LE)"},  // 10-bit sample depth.  
	{name2Id("apcn"), "Apple ProRes 4:2:2 Standard Definition (BE)"},  // 10-bit sample depth.  
	{name2Id("ncpa"), "Apple ProRes 4:2:2 Standard Definition (LE)"},  // 10-bit sample depth.  
	{name2Id("apco"), "Apple ProRes 4:2:2 Proxy (BE)"},  // 10-bit sample depth.  
	{name2Id("ocpa"), "Apple ProRes 4:2:2 Proxy (LE)"},  // 10-bit sample depth.  
	{name2Id("apcs"), "Apple ProRes 4:2:2 Low Target data rate (BE)"},  // 10-bit sample depth.  
	{name2Id("scpa"), "Apple ProRes 4:2:2 Low Target data rate (LE)"},  // 10-bit sample depth.  
	{name2Id("icpf"), "Apple ProRes Intermediate Codec Frame"},
	{name2Id("icod"), "Apple Intermediate Codec"},

	// JPEG 2000 codes.
	{name2Id("asoc"), "Association"},
	{name2Id("bfil"), "Binary Filter"},
	//{name2Id("bpcc"), "Bits Per Component"},
	//{name2Id("cdef"), "Component Definition"},
	{name2Id("cgrp"), "Colour Group"},
	{name2Id("chck"), "Digital signature Check"},
	//{name2Id("cmap"), "Component Mapping"},
	//{name2Id("colr"), "Colour Specification"},
	{name2Id("comp"), "Composition"},
	{name2Id("copt"), "Composition Options"},
	{name2Id("cref"), "Cross-Reference"},
	{name2Id("creg"), "Codestream Registration"},
	{name2Id("drep"), "Desired Reproductions"},
	{name2Id("dtbl"), "Data Table reference"},
	{name2Id("flst"), "Fragment List"},
	//{name2Id("free"), "Free"},
	{name2Id("ftbl"), "Fragment Table"},
	//{name2Id("ftyp"), "File Type"},
	{name2Id("gtso"), "Graphics Technology Standard Output"},
	//{name2Id("ihdr"), "Image Header"},
	{name2Id("inst"), "Instruction Set"},
	//{name2Id("jP  "), "JPEG 2000 signature"},
	//{name2Id("jp2c"), "JPEG 2000 contiguous Codestream"},
	//{name2Id("jp2h"), "JPEG 2000 Header"},
	//{name2Id("jp2i"), "JPEG 2000 Intellectual Property"},
	{name2Id("jpch"), "JPEG Codestream Header"},
	{name2Id("jplh"), "JPEG Compositing Layer Header"},
	{name2Id("lbl "), "Label"},
	//{name2Id("mdat"), "Media Data"},
	//{name2Id("mp7b"), "MPEG-7 Binary"},
	{name2Id("nlst"), "Number List"},
	{name2Id("opct"), "Opacity"},
	//{name2Id("pclr"), "Palette"},
	//{name2Id("prfl"), "Profile"},
	//{name2Id("res "), "Resolution"},
	//{name2Id("resc"), "Capture Resolution"},
	//{name2Id("resd"), "Display Resolution"},
	{name2Id("roid"), "ROI Description"},
	{name2Id("rreq"), "Reader Requirements"},
	//{name2Id("uinf"), "UUID Info"},
	//{name2Id("ulst"), "UUID List"},
	//{name2Id("url "), "URL"},
	//{name2Id("uuid"), "UUID"},  // UUID-EXIF, UUID-EXIF2, UUID-EXIF, UUID-IPTC, UUID-IPTC2, UUID-XMP, UUID-GeoJP2, UUID-Photoshop, UUID-Unknown.

	// Digital Video Codecs (used in digital camcorders).
	{name2Id("dvca"), "DV audio (Casette)"},
	{name2Id("DVSD"), "DV video PAL (SD card)"},
	{name2Id("dv  "), "DV video NTSC"},
	{name2Id("dvc "), "DV video NTSC (Casette)"},
	{name2Id("dvcp"), "DV video PAL (Casette)"},
	{name2Id("dvsd"), "DV video PAL (SD card)"},
	{name2Id("dvp "), "DV video Pro NTSC"},
	{name2Id("dvpp"), "DV video Pro PAL"},
	{name2Id("dv50"), "DV video C Pro 50"},
	{name2Id("dv5p"), "DV video C Pro 50 PAL"},
	{name2Id("dv5n"), "DV video C Pro 50 NTSC"},
	{name2Id("dv1p"), "DV video C Pro 100 PAL"},
	{name2Id("dv1n"), "DV video C Pro 100 NTSC"},
	{name2Id("dvh2"), "DV video 720p24"},
	{name2Id("dvh3"), "DV video 720p25"},
	{name2Id("dvh4"), "DV video 720p30"},
	{name2Id("dvh5"), "DV video C Pro HD 1080i50"},
	{name2Id("dvh6"), "DV video C Pro HD 1080i60"},
	{name2Id("dvhp"), "DV video C Pro HD 720p"},
	{name2Id("hdv1"), "HDV video 720p30 (MPEG-2)"},
	{name2Id("hdv2"), "HDV video 1080i60 (MPEG-2) (Sony)"},
	{name2Id("hdv3"), "HDV video 1080i50 (MPEG-2) (FCP)"},
	{name2Id("hdv4"), "HDV video 720p24 (MPEG-2)"},
	{name2Id("hdv5"), "HDV video 720p25 (MPEG-2)"},
	{name2Id("hdv6"), "HDV video 1080p24 (MPEG-2)"},
	{name2Id("hdv7"), "HDV video 1080p25 (MPEG-2)"},
	{name2Id("hdv8"), "HDV video 1080p30 (MPEG-2)"},
	{name2Id("hdv9"), "HDV video 720p60 (MPEG-2) (JVC)"},
	{name2Id("hdva"), "HDV video 720p50 (MPEG-2)"},

	// AtomicParsley codes.
	{name2Id("<()>"), "Unknown"},
	{name2Id("----"), "Reverse DNS meta-data"},
	{name2Id("mean"), "Reverse DNS domain name"},
	//{name2Id("name"), "Reverse DNS descriptor Name"},  // (Name has multiple uses.)
	{name2Id(".><."), "Child of a data reference"},
	{name2Id("(..)"), "UUID iTunes extension"},

#if 0 // TODO(Hacklin): Find full names in AtomicParsley sources or on the Internet.
	{"drm ",	{"moov"},			CHILD_ATOM,			OPTIONAL_ONCE,		VERSIONED_ATOM },   // 3gp/MobileMP4
	{"tapt",	{"trak"},			PARENT_ATOM,		OPTIONAL_ONE,		SIMPLE_ATOM },
	{"clef",	{"tapt"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },
	{"enof",	{"tapt"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },
	{"gmhd",	{"minf"},			CHILD_ATOM,			REQ_FAMILIAL_ONE,	VERSIONED_ATOM },   //present in chapterized
	{"cios",	{"dref"},			CHILD_ATOM,			OPTIONAL_MANY,		VERSIONED_ATOM },
	{"stps",	{"stbl"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },
	{"stsl",	{"????"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },   //contained by a sample entry box
	{"skcr",	{"sinf"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },
	{"user",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"key ",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },   //could be required in 'drms'/'drmi'
	{"iviv",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"righ",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"name",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"priv",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"iKMS",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },   // 'iAEC', '264b', 'iOMA', 'ICSD'
	{"iSFM",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },
	{"iSLT",	{"schi"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },      //boxes with 'k***' are also here; reserved
	{"IKEY",	{"tref"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"tims",	{"rtp "},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"tsro",	{"rtp "},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"snro",	{"rtp "},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"sdp ",	{"hnti"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"trpy",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"nump",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"tpyl",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"totl",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"npck",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"maxr",	{"hinf"},			CHILD_ATOM,			OPTIONAL_MANY,		SIMPLE_ATOM },
	{"dmed",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"dimm",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"tmin",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"tmax",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"pmax",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"dmax",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"payt",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"tpay",	{"hinf"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"drms",	{"stsd"},			DUAL_STATE_ATOM,	REQ_FAMILIAL_ONE,	VERSIONED_ATOM },
	{"drmi",	{"stsd"},			DUAL_STATE_ATOM,	REQ_FAMILIAL_ONE,	VERSIONED_ATOM },
	{"damr",	{"samr", "sawb"},	CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"d263",	{"s263"},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"dawp",	{"sawp"},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"devc",	{"sevc"},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"dqcp",	{"sqcp"},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"dsmv",	{"ssmv"},			CHILD_ATOM,			REQUIRED_ONE,		SIMPLE_ATOM },
	{"ftab",	{"tx3g"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },
	{"fiel",	{"mjp2"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },      //mjpeg2000
	{"jp2p",	{"mjp2"},			CHILD_ATOM,			OPTIONAL_ONE,		VERSIONED_ATOM },   //mjpeg2000
	{"jsub",	{"mjp2"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },      //mjpeg2000
	{"orfo",	{"mjp2"},			CHILD_ATOM,			OPTIONAL_ONE,		SIMPLE_ATOM },      //mjpeg2000
#endif

	// Nero Burning ROM (taken from AtomicParsley).
	{name2Id("chpl"), "Nero Chapter List"},
	{name2Id("ndrm"), "Nero DRM"},
	{name2Id("tags"), "Nero Tags"},
	{name2Id("tshd"), "Nero Tags Header"},
};


// vim:set ts=4 sw=4 sts=4 noet:
