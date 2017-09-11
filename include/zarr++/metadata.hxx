#pragma once

#include <string>
#include <vector>
#include <boost/any.hpp>
#include "json.hpp"

#ifndef BOOST_FILESYSTEM_NO_DEPERECATED
#define BOOST_FILESYSTEM_NO_DEPERECATED
#endif
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;

#include "zarr++/handle/handle.hxx"
#include "zarr++/types/types.hxx"

namespace zarr {

    struct Metadata {
        const static int zarrFormat = 2;
    };


    struct ArrayMetadata : public Metadata {

        //template<typename T>
        ArrayMetadata(
            const std::string & dtype,
            const types::ShapeType & shape,
            const types::ShapeType & chunkShape,
            const double fillValue=0, // FIXME this should be a template if we use boost::any
            const int compressorLevel=5,
            const std::string & compressorName="lz4",
            const std::string & compressorId="blosc",
            const int compressorShuffle=1,
            const bool isZarr=true
            ) : dtype(types::parseDtype(dtype)),
                shape(shape),
                chunkShape(chunkShape),
                fillValue(static_cast<double>(fillValue)), // FIXME boost::any
                compressorLevel(compressorLevel),
                compressorName(compressorName),
                compressorId(compressorId),
                compressorShuffle(compressorShuffle),
                isZarr(isZarr)
        {
            checkShapes();
        }


        // empty constructur
        ArrayMetadata() : isZarr(true)
        {}


        // json serializable
        void toJson(nlohmann::json & j) const {

            nlohmann::json compressor;
            compressor["clevel"] = compressorLevel;
            compressor["cname"] = compressorName;
            compressor["id"] = (compressorId == "raw") ? nullptr : compressorId;
            compressor["shuffle"] = compressorShuffle;
            j["compressor"] = compressor;

            j["chunks"] = chunkShape;
            j["dtype"] = dtype;

            // FIXME boost::any isn't doing what it's supposed to :(
            //j["fill_value"] = types::isRealType(.dtype) ? boost::any_cast<float>(.fillValue)
            //    : (types::isUnsignedType(.dtype) ? boost::any_cast<uint64_t>(.fillValue)
            //        : boost::any_cast<int64_t>(.fillValue));
            j["fill_value"] = fillValue;

            j["filters"] = filters;
            j["order"] = order;
            j["shape"] = shape;
            j["zarr_format"] = zarrFormat;

        }


        void toJsonN5(nlohmann::json & j) const {
            j["dimensions"] = shape;
            j["blockSize"] = chunkShape;
            j["dataType"] = types::zarrTypeToN5[dtype];
            std::string compressionType;
            if(compressorId == "bzip") {
                compressionType = "bzip2";
            } else if(compressorId == "zlib") {
                if(compressorName != "gzip") {
                    throw std::runtime_error("Compression type is not supported by N5");
                }
                compressionType = "gzip";
            } else if(compressorId == "raw") {
                compressionType = "raw";
            } else {
                throw std::runtime_error("Compression type is not supported by N5");
            }
            j["compressionType"] = compressionType;
        }


        void fromJson(const nlohmann::json & j) {
            checkJson(j);
            dtype = types::parseDtype(j["dtype"]);
            shape = types::ShapeType(j["shape"].begin(), j["shape"].end());
            chunkShape = types::ShapeType(j["chunks"].begin(), j["chunks"].end());
            fillValue = static_cast<double>(j["fill_value"]); // FIXME boost::any
            const auto & compressor = j["compressor"];
            compressorLevel   = compressor["clevel"];
            compressorName    = compressor["cname"];
            compressorId      = compressor["id"].is_null() ? "raw" : compressor["id"];
            compressorShuffle = compressor["shuffle"];
            isZarr = true;
            checkShapes();
        }


        void fromJsonN5(const nlohmann::json & j) {
            dtype = types::n5TypeToZarr[j["dataType"]];
            shape = types::ShapeType(j["dimensions"].begin(), j["dimensions"].end());
            chunkShape = types::ShapeType(j["blockSize"].begin(), j["blockSize"].end());
            const auto & compressionType = j["compressionType"];
            if(compressionType == "gzip") {
                compressorId = "zlib";
                compressorName = "gzip";
            }
            else if(compressionType == "bzip2") {
                compressorId = "bzip";
                compressorName = "bzip";
            } else if(compressionType == "raw") {
                compressorId = "raw";
            } else {
               throw std::runtime_error("Unknow compression type");
            }
            isZarr = false;
            fillValue = 0; // TODO is this correct ?
            checkShapes();
        }


        // metadata values that can be set
        std::string dtype;
        types::ShapeType shape;
        types::ShapeType chunkShape;

        // FIXME boost::any isn't doing what it's supposed to :(
        // for now we just store the fill value as double as a hacky workaround
        //boost::any fillValue;
        double fillValue;

        int compressorLevel;
        std::string compressorName;
        std::string compressorId;
        int compressorShuffle;
        bool isZarr; // flag to specify whether we have a zarr or n5 array

        // metadata values that are fixed for now
        // zarr format is fixed to 2
        const std::string order = "C";
        const std::nullptr_t filters = nullptr;

    private:

        // make sure that shapes agree
        void checkShapes() {
            if(shape.size() != chunkShape.size()) {
                throw std::runtime_error("Dimension of shape and chunks does not agree");
            }
            for(unsigned d = 0; d < shape.size(); ++d) {
                if(chunkShape[d] > shape[d]) {
                    throw std::runtime_error("Chunkshape cannot be bigger than shape");
                }
            }
        }


        // make sure that fixed metadata values agree
        void checkJson(const nlohmann::json & j) {

            // check if order exists and check for the correct value
            auto jIt = j.find("order");
            if(jIt != j.end()) {
                if(*jIt != order) {
                    throw std::runtime_error(
                        "Invalid Order: Zarr++ only supports C order"
                    );
                }
            }

            jIt = j.find("zarr_format");
            if(jIt != j.end()) {
                if(*jIt != zarrFormat) {
                    throw std::runtime_error(
                        "Invalid Zarr format: Zarr++ only supports zarr format 2"
                    );
                }
            }

            jIt = j.find("filters");
            if(jIt != j.end()) {
                if(!j["filters"].is_null()) {
                    throw std::runtime_error(
                        "Invalid Filters: Zarr++ does not support filters"
                    );
                }
            }
        }
    };


    //
    // helper functions
    //

    void writeMetadata(const handle::Array & handle, const ArrayMetadata & metadata) {
        fs::path filePath = handle.path();
        nlohmann::json j;
        if(metadata.isZarr) {
            filePath /= ".zarray";
            metadata.toJson(j);
        }
        else {
            filePath /= "attributes.json";
            metadata.toJsonN5(j);
        }
        fs::ofstream file(filePath);
        file << std::setw(4) << j << std::endl;
        file.close();
    }

    bool getMetadataPath(const handle::Array & handle, fs::path & path) {
        fs::path zarrPath = handle.path();
        fs::path n5Path = handle.path();
        zarrPath /= ".zarray";
        n5Path /= "attributes.json";
        if(fs::exists(zarrPath) && fs::exists(n5Path)) {
            throw std::runtime_error("Zarr and N5 specification are not both supported");
        }
        if(!fs::exists(zarrPath) && !fs::exists(n5Path)){
            throw std::runtime_error("Invalid path: no metadata existing");
        }
        bool isZarr = fs::exists(zarrPath);
        path = isZarr ? zarrPath : n5Path;
        return isZarr;
    }

    void readMetadata(const handle::Array & handle, ArrayMetadata & metadata) {
        nlohmann::json j;
        fs::path filePath;
        auto isZarr = getMetadataPath(handle, filePath);
        fs::ifstream file(filePath);
        file >> j;
        if(isZarr) {
            metadata.fromJson(j);
        } else {
            metadata.fromJsonN5(j);
        }
        file.close();
    }

    std::string readDatatype(const handle::Array & handle) {
        fs::path filePath;
        bool isZarr = getMetadataPath(handle, filePath);
        fs::ifstream file(filePath);
        nlohmann::json j;
        file >> j;
        file.close();
        return isZarr ? j["dtype"] : j["dataType"];
    }

} // namespace::zarr
