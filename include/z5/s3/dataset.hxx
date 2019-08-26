#pragma once
#include "z5/dataset.hxx"
#include "z5/s3/handle.hxx"


namespace z5 {
namespace s3 {

    // TODO implement for s3

    template<typename T>
    class Dataset : public z5::Dataset, private z5::MixinTyped<T> {

    public:
        typedef T value_type;
        typedef types::ShapeType shape_type;
        typedef z5::MixinTyped<T> Mixin;

        // create a new array with metadata
        Dataset(const handle::Dataset & handle,
                const DatasetMetadata & metadata) : z5::Dataset(metadata),
                                                    Mixin(metadata),
                                                    handle_(handle){
        }

        //
        // Implement Dataset API
        //

        inline void writeChunk(const types::ShapeType & chunkIndices, const void * dataIn,
                               const bool isVarlen=false, const std::size_t varSize=0) const {
            // check if we are allowed to write
            if(!handle_.mode().canWrite()) {
                const std::string err = "Cannot write data in file mode " + handle_.mode().printMode();
                throw std::invalid_argument(err.c_str());
            }
        }


        // read a chunk
        // IMPORTANT we assume that the data pointer is already initialized up to chunkSize_
        inline bool readChunk(const types::ShapeType & chunkIndices, void * dataOut) const {
            // get the chunk handle
            handle::Chunk chunk(handle_, chunkIndices, defaultChunkShape(), shape());

            // TODO implement
            // make sure that we have a valid chunk
            // checkChunk(chunk);

            // throw runtime errror if trying to read non-existing chunk
            if(!chunk.exists()) {
                throw std::runtime_error("Trying to read a chunk that does not exist");
            }

            // load the data from disc
            std::vector<char> buffer;
            read(chunk, buffer);

            // format the data
            const bool is_varlen = util::buffer_to_data<T>(chunk, buffer, dataOut, Mixin::compressor_);

            return is_varlen;
        }


        inline void checkRequestType(const std::type_info & type) const {
            if(type != typeid(T)) {
                // TODO all in error message
                std::cout << "Mytype: " << typeid(T).name() << " your type: " << type.name() << std::endl;
                throw std::runtime_error("Request has wrong type");
            }
        }


        inline bool chunkExists(const types::ShapeType & chunkId) const {
        }
        inline std::size_t getChunkSize(const types::ShapeType & chunkId) const {
        }
        inline void getChunkShape(const types::ShapeType & chunkId, types::ShapeType & chunkShape) const {
        }
        inline std::size_t getChunkShape(const types::ShapeType & chunkId, const unsigned dim) const {
        }


        // compression options
        inline types::Compressor getCompressor() const {return Mixin::compressor_->type();}
        inline void getCompressor(std::string & compressor) const {
            auto compressorType = getCompressor();
            compressor = isZarr_ ? types::Compressors::compressorToZarr()[compressorType] : types::Compressors::compressorToN5()[compressorType];
        }

        inline void getFillValue(void * fillValue) const {
            *((T*) fillValue) = Mixin::fillValue_;
        }


        inline bool checkVarlenChunk(const types::ShapeType & chunkId, std::size_t & chunkSize) const {
        }

        inline const FileMode & mode() const {
        }
        inline const fs::path & path() const {
        }
        inline void chunkPath(const types::ShapeType & chunkId, fs::path & path) const {
        }
        inline void removeChunk(const types::ShapeType & chunkId) const {
        }
        inline void remove() const {
            handle_.remove();
        }
        inline void getCompressionOptions(types::CompressionOptions & opts) const {
            Mixin::compressor_->getOptions(opts);
        }

        // delete copy constructor and assignment operator
        // because the compressor cannot be copied by default
        // and we don't really need this to be copyable afaik
        // if this changes at some point, we need to provide a proper
        // implementation here
        Dataset(const Dataset & that) = delete;
        Dataset & operator=(const Dataset & that) = delete;

    private:

        inline void read(const handle::Chunk & chunk, std::vector<char> & buffer) const {
            // TODO read from s3 onject corresponding to chunk into the buffer
        }

        handle::Dataset handle_;
    };


}
}
