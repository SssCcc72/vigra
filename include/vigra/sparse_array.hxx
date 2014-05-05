/************************************************************************/
/*                                                                      */
/*               Copyright 2014 by Thorsten Beier                       */
/*                                                                      */
/*    This file is part of the VIGRA computer vision library.           */
/*    The VIGRA Website is                                              */
/*        http://hci.iwr.uni-heidelberg.de/vigra/                       */
/*    Please direct questions, bug reports, and contributions to        */
/*        ullrich.koethe@iwr.uni-heidelberg.de    or                    */
/*        vigra@informatik.uni-hamburg.de                               */
/*                                                                      */
/*    Permission is hereby granted, free of charge, to any person       */
/*    obtaining a copy of this software and associated documentation    */
/*    files (the "Software"), to deal in the Software without           */
/*    restriction, including without limitation the rights to use,      */
/*    copy, modify, merge, publish, distribute, sublicense, and/or      */
/*    sell copies of the Software, and to permit persons to whom the    */
/*    Software is furnished to do so, subject to the following          */
/*    conditions:                                                       */
/*                                                                      */
/*    The above copyright notice and this permission notice shall be    */
/*    included in all copies or substantial portions of the             */
/*    Software.                                                         */
/*                                                                      */
/*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND    */
/*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES   */
/*    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND          */
/*    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT       */
/*    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,      */
/*    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING      */
/*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR     */
/*    OTHER DEALINGS IN THE SOFTWARE.                                   */                
/*                                                                      */
/************************************************************************/

#ifndef VIGRA_SPARSE_ARRAY
#define VIGRA_SPARSE_ARRAY


#include <map>


namespace vigra{
namespace sparse{


    template<class SA>
    class SparseReturnProxy{
    public:
        //typedef typename SA::MapType MapType;
        //typedef typename SA::ConstMapIter ConstMapIter;
        //typedef typename SA::MapIter MapIter;
        typedef typename SA::value_type value_type;
        typedef value_type & reference;
        typedef typename SA::coordinate_value_pair coordinate_value_pair;
        typedef typename SA::MapType MapType;
        typedef typename SA::MapIter MapIter;

        SparseReturnProxy(SA * sa = NULL,const size_t index = 0):
        sa_(sa),
        index_(index){

        }


        // write access
        // - convert from value_type to proxy
        SparseReturnProxy & operator=( const value_type & value ) {

            MapIter lb = sa_->storage_.lower_bound(index_); 
            if(lb != sa_->storage_.end() && !(sa_->storage_.key_comp()(index_, lb->first)))
                lb->second=value; 
            else 
                sa_->storage_.insert(lb, typename MapType::value_type(index_, value));
            return *this;
        }

        // read access
        // - convert from proxy to value_type
        operator value_type()const{
            return sa_->read(index_);
        }
        // macro to make ++ , ++(int) , --, --(int)
        // +=  and  -=  are also generated
        #define VIGRA_SPARSE_MAKE_OP_MACRO(OP,OP_PRE,OP_POST,SIG) \
        SparseReturnProxy<SA> & operator OP (SIG){ \
            MapIter lb = sa_->storage_.lower_bound(index_); \
            if(lb != sa_->storage_.end() && !(sa_->storage_.key_comp()(index_, lb->first))) \
                OP_PRE lb->second OP_POST ; \
            else \
            { \
                value_type val = sa_->zeroValue(); \
                OP_PRE val OP_POST; \
                sa_->storage_.insert(lb, typename MapType::value_type(index_, val)); \
            } \
            return *this; \
        }  
        VIGRA_SPARSE_MAKE_OP_MACRO(++,++, , void)
        VIGRA_SPARSE_MAKE_OP_MACRO(++,  ,++,int)
        VIGRA_SPARSE_MAKE_OP_MACRO(--,--, , void)
        VIGRA_SPARSE_MAKE_OP_MACRO(--,  ,--,int)
        VIGRA_SPARSE_MAKE_OP_MACRO(+=, ,+=value, const value_type value)
        VIGRA_SPARSE_MAKE_OP_MACRO(-=, ,-=value, const value_type value)
        #undef VIGRA_SPARSE_MAKE_OP_MACRO

        #define VIGRA_SPARSE_MAKE_OP_MACRO(OP,OP_PRE,OP_POST,SIG) \
        SparseReturnProxy<SA> & operator OP (SIG){ \
            MapIter lb = sa_->storage_.lower_bound(index_); \ 
            if(lb != sa_->storage_.end() && !(sa_->storage_.key_comp()(index_, lb->first))) \
                OP_PRE lb->second OP_POST ; \
            return *this; \
        }  
        VIGRA_SPARSE_MAKE_OP_MACRO(*=, ,*=value, const value_type value)
        VIGRA_SPARSE_MAKE_OP_MACRO(/=, ,/=value, const value_type value)
        #undef VIGRA_SPARSE_MAKE_OP_MACRO
    private:
        SA * sa_;
        size_t index_;
    };




    template<class T ,class EQUAL_COMP = std::equal_to<T> >
    class SparseMapVector
    {


    private:
        typedef SparseMapVector<T,EQUAL_COMP> SelfType;
        typedef SparseReturnProxy<SelfType> Proxy;
        typedef EQUAL_COMP EqualCompare;
        friend class SparseReturnProxy<SparseMapVector<T> >;

        typedef std::map<size_t,T> MapType;
        typedef typename MapType::const_iterator ConstMapIter;
        typedef typename MapType::iterator MapIter;
        
    public:
        typedef std::pair<size_t,T> coordinate_value_pair;
        typedef size_t coordinate_type;
        typedef T value_type;
        typedef const T & const_reference;



        Proxy operator()(const size_t index){
            return Proxy(this,index);
        }

        Proxy operator[](const size_t index){
            return Proxy(this,index);
        }


        const_reference operator()(const size_t index)const{
            return read(index);
        }

        const_reference operator[](const size_t index)const{
            return read(index);
        }

        const SelfType & asConst()const{
            return *this;
        }   





        SparseMapVector(const size_t size=0, const T & zeroValue  = T(0),
                        const EqualCompare & eqComp = EqualCompare())
        :   size_(size),
            storage_(),
            zeroValue_(zeroValue),
            eqComp_(eqComp)
        {

        }

        template<class INDEX_VALUE_PAIR_ITER>
        SparseMapVector(const size_t size,INDEX_VALUE_PAIR_ITER indexValuePairBegin,
                        INDEX_VALUE_PAIR_ITER indexValuePairEnd,
                        const T & zeroValue  = T(0),
                        const EqualCompare & eqComp = EqualCompare() )
        :   size_(size),
            storage_(indexValuePairBegin,indexValuePairEnd),
            zeroValue_(zeroValue),
            eqComp_(eqComp)
        {

        }


        template<class ITER_INDEX , class ITER_VALUE>
        SparseMapVector(const size_t size, ITER_INDEX indexBegin,
                        ITER_INDEX indexEnd,ITER_VALUE valueBegin,
                        const T & zeroValue  = T(0),
                        const EqualCompare & eqComp = EqualCompare())
        :   size_(size),
            storage_(),
            zeroValue_(zeroValue),
            eqComp_(eqComp)
        {
            const size_t nVals = std::distance(indexBegin,indexEnd);
            std::vector<coordinate_value_pair> indexValuePairVec(nVals);
            for(size_t i=0;i<nVals;++i){
                coordinate_value_pair & mp = indexValuePairVec[i];
                mp.first=*indexBegin;
                mp.second=*valueBegin;
                ++indexBegin;
                ++valueBegin;
            }
            MapType tmp(indexValuePairVec.begin(),indexValuePairVec.end());
            storage_.swap(tmp);
        }



        const T & read(const size_t index)const{
            const ConstMapIter iter = storage_.find(index);
            return iter==storage_.end() ? zeroValue_ : iter->second;
        }

        T & writableRef(const size_t index){
           return storage_.insert(coordinate_value_pair(index,zeroValue())).first->second;
        }

        size_t size()const{
            return size_;
        }

        const_reference zeroValue()const{
            return zeroValue_;
        }


        void  swap(SelfType & other){
            storage_.swap(other.storage_);
            std::swap(size_,other.size_);
            std::swap(zeroValue_,other.zeroValue_);
            std::swap(eqComp_,other.eqComp_);
        }

    private:

        MapIter find(const size_t index){
            return storage_.find(index);
        }
        ConstMapIter find(const size_t index)const{
            return storage_.find(index);
        }


        MapIter storageEnd(){
            return storage_.end();
        }
        ConstMapIter storageEnd()const{
            return storage_.end();
        }


        size_t size_; 
        MapType storage_;
        T zeroValue_;
        EqualCompare eqComp_;
    };


} // end name sparse
} // end namespace vigra

#endif /* VIGRA_SPARSE_ARRAY */
