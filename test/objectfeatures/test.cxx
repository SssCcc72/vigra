#include <iostream>
#include <sstream>
#include <map>
#include <set>

#include "unittest.hxx"

#include "vigra/accessor.hxx"
#include "vigra/tinyvector.hxx"
#include "vigra/rgbvalue.hxx"

#include "vigra/coordinate_iterator.hxx"
#include "vigra/object_features.hxx"

#include "vigra/multi_pointoperators.hxx"

#include "vigra/basicimage.hxx"
#include "vigra/stdimage.hxx" // BImage
#include "vigra/inspectimage.hxx" // FindAverageAndVariance

#include "vigra/multi_array.hxx"
#include "vigra/multi_convolution.hxx"
#include "vigra/impex.hxx"
#include "vigra/imageinfo.hxx"
#include "vigra/functorexpression.hxx"


using namespace vigra;

template <class X, unsigned N>
TinyVector<X, N> & tab_assign(TinyVector<X, N> & x, const X y[N])
{
    for (unsigned k = 0; k != N; ++k)
        x[k] = y[k];
    return x;
}

template <class X>
X nan_squash(const X & y)
{
    if (y == y)
        return y;
    else
        return X(-0.0);
}
template <class X, unsigned N>
TinyVector<X, N> nan_squash(const TinyVector<X, N> & y)
{
    TinyVector<X, N> x;
    for (unsigned k = 0; k != N; ++k)
        x[k] = y[k] == y[k] ? y[k] : -0.0;
    return x;
}

using vigra::type_lists::use_template_list;
using vigra::Accumulators;

typedef double pixel_type;

typedef vigra::MultiArrayShape<2>::type  shape_2d;
typedef vigra::MultiArray<2, pixel_type> array_2d;

typedef vigra::RGBValue<double> rgb_type;

template <class T>
struct gen_accumulators
    : public use_template_list<Accumulators, T,
                               acc::Variance,
                               acc::Skewness,
                               acc::Kurtosis
                              >
{};

typedef gen_accumulators<pixel_type> pixel_accumulators;

typedef use_template_list<Accumulators, StridePairPointer<2, pixel_type>,
                          acc::Count,
                          acc::Sum,
                          acc::CoordSum
                         >
cv_test_accumulators;


typedef use_template_list<Accumulators, pixel_type,
                          acc::Count,
                          acc::Sum,
                          acc::Mean,
                          acc::Moment2
                         >
yy_test_accumulators;

typedef use_template_list<Accumulators, pixel_type,
                          acc::Mean,
                          acc::Moment2
                         >
yyy_test_accumulators;

struct count : public InitializerTag
{
    mutable unsigned long t;
    count() : t(0) {}
    unsigned long operator()() const
    {
        return t++;
    }
};

void test1()
{
    pixel_accumulators pix_acc;
    yyy_test_accumulators yy_acc;
    
	array_2d image(array_2d::size_type(10, 10));

    for (unsigned i = 0; i != 100; ++i)
        image(i / 10, i % 10) = i;

	inspectMultiArray(srcMultiArrayRange(image), pix_acc);

    shouldEqual(get<acc::Count>(pix_acc), 100);
    shouldEqualTolerance(get<acc::Mean>(pix_acc), 49.5, 2e-15);
    shouldEqualTolerance(get<acc::Moment2>(pix_acc), 83325, 2e-15);
    shouldEqualTolerance(get<acc::Moment2_2>(pix_acc), 83325, 2e-15);
    shouldEqualTolerance(get<acc::Variance>(pix_acc), 833.25, 2e-15);
    shouldEqualTolerance(get<acc::Skewness>(pix_acc), 0, 2e-15);
    shouldEqualTolerance(get<acc::Kurtosis>(pix_acc), 0.017997599759976, 2e-15);
}

void test2()
{
	array_2d stride_image2(array_2d::size_type(2, 3));
    initMultiArray(destMultiArrayRange(stride_image2), count());

    cv_test_accumulators cv_acc;

    inspectMultiArray(srcCoordinateMultiArrayRange(stride_image2), cv_acc);

    shouldEqual(get<acc::Count>(cv_acc), 6);
    shouldEqual(get<acc::Sum>(cv_acc), 15);
    { TinyVector<double, 2> x(3, 6); shouldEqualSequence(x.begin(), x.end(), get<acc::CoordSum>(cv_acc).begin()); };
    rgb_type epstest = 1e-5;
    { TinyVector<double, 3> x(1e-05, 1e-05, 1e-05); shouldEqualSequenceTolerance(x.begin(), x.end(), epstest.begin(), 2e-15); };
}


using vigra::NestedAccumulators;
using vigra::SelectedAccumulators;

template <class T, template<class> class CV,
                   template<class> class SUM>
struct gen_mean_from_sum_calc
    : public detail::calculator_base<typename CV<T>::type>,
      public type_lists::implies_template<T, SUM, acc::Count>
{
    template <class ACX>
    void calculate(const ACX & x) const // calculator tuple == x.calculators
    {
        this->value = detail::use<SUM>::val(x)
                      / detail::use<acc::Count>::val(x);
    }
};
template <class T>
struct mean_from_sum_calc
    : public gen_mean_from_sum_calc<T, detail::accumulable_value_access,
                                       acc::Sum> {};
template <class T>
struct coord_mean_from_sum_calc
    : public gen_mean_from_sum_calc<T, detail::accumulable_coord_access,
                                       acc::CoordSum>
{};

// testing contexts:

template <class T, class L>
struct cond_plain_accu_context
    : public detail::accu_context_base<T, L, type_lists::cond_tuple_plain> {};

template <class T, class L>
struct cond_accu_context
    : public detail::accu_context_base<T, L, type_lists::cond_tuple> {};

template <class T, class L>
struct virtual_accu_context
    : public detail::accu_context_base<T, L,
              type_lists::virtual_tuple<virtual_accu_context, T,
                                        detail::v_feature_dispatch>::template
                                                                     type>
{};

template <class Z> const Z & make_const(const Z & z) { return z; }

template <class T>
struct mean_context
    : public use_template_list<Accumulators, T, acc::Count, acc::Mean>
{
};

template <class T>
struct nested_accumulators_mean
    : public use_template_list<NestedAccumulators, T,
                                    acc::Count, acc::Mean>
{
};

template <class T>
struct selected_accumulators_mean
    : public use_template_list<SelectedAccumulators, T, acc::Mean>
{
};

// sum of squares

template <class T>
struct accumulators_sq
    : public use_template_list<Accumulators, T, acc::Moment2>
{
};

template <class T>
struct accumulators_sq2
    : public use_template_list<Accumulators, T, acc::Variance,
                                                     acc::UnbiasedVariance>
{
};

template <class T>
struct nested_accumulators_sq
    : public use_template_list<NestedAccumulators, T,
                                    acc::Moment2>
{
};

template <class T>
struct selected_accumulators_sq
    : public use_template_list<SelectedAccumulators, T,
                                    acc::Moment2>
{
};

template <class T>
struct cond_plain_mean_context
    : public use_template_list<cond_plain_accu_context, T,
                                    acc::Count, acc::Mean>
{
};

template <class T>
struct cond_mean_context
    : public use_template_list<cond_accu_context, T,
                                    acc::Count, acc::Mean>
{
};

template <class T>
struct accumulators_mean
    : public use_template_list<Accumulators, T, acc::Mean>
{
};

template <class T>
struct virtual_mean_context
    : public use_template_list<virtual_accu_context, T, acc::Mean>
{
};

template <class T>
struct accumulators_mean_sum
    : public use_template_list<Accumulators, T, acc::Sum, mean_from_sum_calc,
                               acc::Moment2_2, acc::Variance>
{};

template <class key, class data>
struct gen_store {
    typedef std::map<key, data> gen_map;
    typedef typename gen_map::iterator    iterator;
    typedef typename gen_map::size_type    size_type;
    typedef typename gen_map::mapped_type    data_type; 
    typedef typename gen_map::key_type    key_type;
    typedef typename gen_map::value_type    value_type;
    typedef typename gen_map::const_iterator    const_iterator;
    typedef pair<data, bool> join_type;

    gen_map store_map;

    bool find(const key & x, data & id) const {
        const_iterator i = store_map.find(x);
        bool found = (i != store_map.end());
        if (found)
            id = (*i).second;
        return found;
    }
    bool exists(const key & x) const {
        return store_map.find(x) != store_map.end();
    }
};

template <class search, class id_type = unsigned>
class label_map
{
private:
    typedef std::map<search, id_type>        gen_map;
    typedef typename gen_map::iterator       iterator;
    typedef typename gen_map::size_type      size_type;
    typedef typename gen_map::mapped_type    data_type; 
    typedef typename gen_map::key_type       key_type;
    typedef typename gen_map::value_type     value_type;
    typedef typename gen_map::const_iterator const_iterator;
	typedef          search                  reverse_type;
    typedef pair<id_type, bool> join_type;

    gen_map                   store_map;
    id_type                   id_count;
    std::vector<reverse_type> reverse_map;

public:
    bool find(const search & x, id_type & id) const
    {
        const_iterator i = store_map.find(x);
        bool found = (i != store_map.end());
        if (found)
            id = (*i).second;
        return found;
    }
    bool exists(const search & x) const
    {
        return store_map.find(x) != store_map.end();
    }
private:
    id_type push_back(const search & x = search())
    {
        reverse_map.push_back(x);
        return id_count++;
    }
    join_type join(const search & x)
    {
        std::pair<iterator, bool> inserted =
            store_map.insert(value_type(x, id_count));
        if (inserted.second)
            push_back(x);
        // return (id number, was it new?)
        return join_type((*(inserted.first)).second, inserted.second);
    }
public:
    label_map() : id_count(0) {}
    void clear()
    {
        id_count = 0;
        store_map.clear();
        reverse_map.clear();
    }
    id_type count() const { return id_count; }
    // adds x into symbol map if new, returns id #
    id_type operator()(const search & x)
    {
        return join(x).first;
    }
    // transform to colour label
    unsigned operator()(const search & v) const
    {
        unsigned id;
        if (find(v, id))
        {
            return id;
        }
        else
            return ~(unsigned(0));
    }
    // adds x into symbol map if new, returns "if new" and sets id
    bool subjoin(const search & x, id_type & id)
    {
        join_type j = join(x);
        id = j.first;
        return j.second;
    }
    bool subjoin(const search & x)
    {
        join_type j = join(x);
        return j.second;
    }
    const reverse_type & operator[](id_type id) const
    {
        return reverse_map[id];
    }
    reverse_type & operator[](id_type id)
    {
        return reverse_map[id];
    }
};

template <class T>
struct colour_label_acc : public detail::collector_base<T, label_map<T> >
{
    bool test_member;
    colour_label_acc() : test_member(false) {}
    
    typedef typename colour_label_acc::collector_base_type collector_base_type;
    void reset()
    {
        this->value.clear();
    }
    // reset() and unimplemented variants
    using collector_base_type::operator();
    /** update
    */
    template <class ACX>
    void updatePass1(ACX &, const T & v)
    {
        this->value(v);
    }
    /** merge two statistics: z === *this <- (x, y)
    */
    template <class ACX>
    void operator()(const ACX & z, const ACX & x, const ACX & y);
};

typedef vigra::RGBValue<double> vpixel_type;

typedef vigra::MultiArrayShape<2>::type                          shape_v2d;
typedef vigra::MultiArray<2, vpixel_type>                        array_v2d;
typedef vigra::MultiArray<2, int>                                labels_v2d;
typedef vigra::MultiArray<2, vigra::TinyVector<vpixel_type, 2> > grad_v2d;
typedef vigra::MultiArray<2, vigra::TinyVector<vpixel_type, 3> > symm_v2d;
typedef vigra::BasicImageView<vpixel_type>                       image_v2d;
typedef vigra::ConvolutionOptions<2>                             options_v2d;

template <class T>
struct accumulators_colour_label
    : public use_template_list<Accumulators, T, colour_label_acc>
{};

void test3()
{
    BImage img(10, 10);
    for (unsigned i = 0; i != 100; ++i)
        img(i / 10, i % 10) = i;

    FindAverageAndVariance<BImage::PixelType> averageAndVariance; // init functor
    mean_context<BImage::PixelType> my_mean;

    cond_plain_mean_context<BImage::PixelType>    my_c_mean;
    cond_mean_context<BImage::PixelType>          my_c_mean_new;
    nested_accumulators_mean<BImage::PixelType>   my_c_mean_bit;
    selected_accumulators_mean<BImage::PixelType> my_v_mean; 
    accumulators_mean<BImage::PixelType>          test_mean;
    cond_plain_mean_context<BImage::PixelType>    test_c_mean;
    cond_mean_context<BImage::PixelType>          test_c_mean_new;
    nested_accumulators_mean<BImage::PixelType>   test_c_mean_bit;
    selected_accumulators_mean<BImage::PixelType> test_v_mean;

    accumulators_mean_sum<BImage::PixelType>      mean_sum_0;
    accumulators_colour_label<BImage::PixelType>  colour_0;

    virtual_mean_context<BImage::PixelType> virtual_meanest;
    detail::use<acc::Mean>::set(virtual_meanest);

    virtual_mean_context<BImage::PixelType> virtual_empty;

    accumulators_sq<BImage::PixelType> my_sq;
    accumulators_sq2<BImage::PixelType> my_sq2;

    shouldEqual(detail::passes<1>::at_least<acc::Count<BImage::PixelType> >::value, 1);
    shouldEqualTolerance(detail::passes<1>::at_least<acc::Variance<BImage::PixelType> >::value, 0, 2e-15);
    
    shouldEqualTolerance(detail::use<acc::Count>::f_val(my_sq), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_sq), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Moment2>::f_val(my_sq), 0, 2e-15);

    shouldEqualTolerance(detail::use<acc::Count>::f_val(my_c_mean), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Count>::f_val(my_c_mean_new), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean_new), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Count>::f_val(my_c_mean_bit), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean_bit), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Count>::val(my_v_mean), 0, 2e-15); //getting virtual stuff
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);  //sets them, thus val

    detail::use<acc::Count>::set(my_c_mean); // only use count...
    detail::use<acc::Count>::set(my_c_mean_new); // only use count...
    detail::use<acc::Count>::set(my_c_mean_bit); // only use count...
    select<acc::Count>(my_v_mean); // only use count...
    
    detail::use<acc::Mean>::set(test_c_mean);
    detail::use<acc::Mean>::set(test_v_mean);
    detail::use<acc::Count>::set(test_v_mean); // test idempotence
    detail::use<acc::Mean>::set(test_c_mean_new);
    detail::use<acc::Mean>::set(test_c_mean_bit);

    inspectImage(srcImageRange(img), averageAndVariance);
    inspectImage(srcImageRange(img), my_sq);
    inspectImage(srcImageRange(img), my_sq2);
    inspectImage(srcImageRange(img), my_mean);
    inspectImage(srcImageRange(img), my_c_mean);
    inspectImage(srcImageRange(img), my_c_mean_new);
    inspectImage(srcImageRange(img), my_c_mean_bit);
    inspectImage(srcImageRange(img), my_v_mean);
    inspectImage(srcImageRange(img), test_mean);
    inspectImage(srcImageRange(img), test_c_mean);
    inspectImage(srcImageRange(img), test_c_mean_new);
    inspectImage(srcImageRange(img), test_c_mean_bit);
    inspectImage(srcImageRange(img), test_v_mean);

    inspectImage(srcImageRange(img), mean_sum_0);
    inspectImage(srcImageRange(img), colour_0);

    inspectImage(srcImageRange(img), virtual_meanest);
    inspectImage(srcImageRange(img), virtual_empty);

    shouldEqual(averageAndVariance.count_, 100);
    shouldEqualTolerance(averageAndVariance.mean_, 49.5, 2e-15);
    shouldEqualTolerance(averageAndVariance.sumOfSquaredDifferences_, 83325, 2e-15);
 
    shouldEqual(detail::use<acc::Count>::f_val(my_sq2), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_sq2), 49.5, 2e-15);
    shouldEqualTolerance(detail::use<acc::Moment2>::f_val(my_sq2), 83325, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(my_sq2), 833.25, 2e-15);
    shouldEqualTolerance(detail::use<acc::UnbiasedVariance>::val(my_sq2), 841.666666666666, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(my_sq), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_sq), 49.5, 2e-15);
    shouldEqualTolerance(detail::use<acc::Moment2>::f_val(my_sq), 83325, 2e-15);
    shouldEqualTolerance(std::sqrt(detail::use<acc::Moment2>::f_val(my_sq) / detail::use<acc::Count>::f_val(my_sq)), 28.8660700477221, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(my_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_mean), 49.5, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(my_c_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(my_c_mean_new), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean_new), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(my_c_mean_bit), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_c_mean_bit), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(test_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(test_mean), 49.5, 2e-15);
    
    shouldEqual(detail::use<acc::Count>::f_val(test_c_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(test_c_mean), 49.5, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(test_c_mean_new), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(test_c_mean_new), 49.5, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(test_c_mean_bit), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(test_c_mean_bit), 49.5, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(test_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(test_v_mean), 49.5, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(virtual_meanest), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(virtual_meanest), 49.5, 2e-15);

    shouldEqualTolerance(detail::use<acc::Count>::val(virtual_empty), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::val(virtual_empty), 0, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);

    mean_context<BImage::PixelType> my_mean2 = my_mean;
    cond_plain_mean_context<BImage::PixelType> my_c_mean2 = my_c_mean;
    cond_mean_context<BImage::PixelType> my_c_mean_new2 = my_c_mean_new;
    nested_accumulators_mean<BImage::PixelType> my_c_mean_bit2 = my_c_mean_bit;

    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);

    my_mean2(my_mean2, my_mean);
    shouldEqual(detail::use<acc::Count>::f_val(my_mean2), 200);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_mean2), 49.5, 2e-15);
    mean_context<BImage::PixelType> my_mean3;
    my_mean3(my_mean, my_mean3);
    shouldEqual(detail::use<acc::Count>::f_val(my_mean3), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_mean3), 49.5, 2e-15);
    mean_context<BImage::PixelType> my_mean4(my_mean3);
    my_mean4(my_mean2);
    shouldEqual(detail::use<acc::Count>::f_val(my_mean4), 300);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_mean4), 49.5, 2e-15);
    my_mean4.reset();
    shouldEqualTolerance(detail::use<acc::Count>::f_val(my_mean4), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_mean4), 0, 2e-15);

    mean_context<BImage::PixelType> my_mean5;
    my_mean4.transfer_set_to(my_mean5);

    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);

    selected_accumulators_mean<BImage::PixelType> my_v_mean2 = my_v_mean;
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean2), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean2), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(test_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(test_v_mean), 49.5, 2e-15);

    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean2), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean2), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);
    my_v_mean2(test_v_mean);
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean2), 200);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_v_mean2), 24.75, 2e-15); // not a bug, but a feature..

    shouldEqual(detail::use<acc::Count>::f_val(test_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(test_v_mean), 49.5, 2e-15);

    shouldEqual(detail::use<acc::Count>::val(my_v_mean), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(my_v_mean), 0, 2e-15);
    my_v_mean(test_v_mean);
    shouldEqual(detail::use<acc::Count>::f_val(my_v_mean), 200);
    shouldEqualTolerance(detail::use<acc::Mean>::f_val(my_v_mean), 24.75, 2e-15); // not a bug, but a feature..

    typedef selected_accumulators_mean<BImage::PixelType> cvmc;
    cvmc* my_v_mean3 = new cvmc(my_v_mean);
    shouldEqual(detail::use<acc::Count>::val(*my_v_mean3), 200);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean3), 24.75, 2e-15);
    my_v_mean3->reset();
    shouldEqualTolerance(detail::use<acc::Count>::val(*my_v_mean3), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean3), 0, 2e-15);
    (*my_v_mean3)(test_v_mean);
    shouldEqual(detail::use<acc::Count>::val(*my_v_mean3), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean3), 49.5, 2e-15);
    cvmc* my_v_mean4 = new cvmc;
    shouldEqualTolerance(detail::use<acc::Count>::val(*my_v_mean4), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean4), 0, 2e-15);
    cvmc* my_v_mean5 = new cvmc(*my_v_mean3);
    delete my_v_mean3;
    shouldEqual(detail::use<acc::Count>::val(*my_v_mean5), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean5), 49.5, 2e-15);
    (*my_v_mean4)(*my_v_mean5, *my_v_mean5);
    shouldEqual(detail::use<acc::Count>::val(*my_v_mean4), 200);
    shouldEqualTolerance(detail::use<acc::Mean>::val(*my_v_mean4), 49.5, 2e-15);

    shouldEqual(detail::use<acc::Count>::val(mean_sum_0), 100);
    shouldEqual(detail::use<acc::Sum>::val(mean_sum_0), 4950);
    shouldEqualTolerance(detail::use<mean_from_sum_calc>::c_val(mean_sum_0.calculators), 0, 2e-15);
    shouldEqualTolerance(detail::use<mean_from_sum_calc>::val(mean_sum_0), 49.5, 2e-15);
    shouldEqualTolerance(detail::use<mean_from_sum_calc>::c_val(mean_sum_0.calculators), 49.5, 2e-15);
    shouldEqual(accumulators_mean_sum<BImage::PixelType>::max_passes, 2);
    shouldEqual(detail::use<acc::Moment2_2>::val(mean_sum_0), 83325);

    shouldEqualTolerance(detail::use<acc::Variance>::c_val(mean_sum_0.calculators), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(mean_sum_0), 833.25, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::c_val(mean_sum_0.calculators), 833.25, 2e-15);

    shouldEqual(detail::use<colour_label_acc>::val(colour_0).count(), 100);

    shouldEqual(get<colour_label_acc>(colour_0).count(), 100);
    shouldEqualTolerance(member<colour_label_acc>(colour_0).test_member, 0, 2e-15);
    member<colour_label_acc>(colour_0).test_member = true;
    shouldEqualTolerance(member<colour_label_acc>(make_const(colour_0)).test_member, 1, 2e-15);
}

void test4()
{
	typedef MultiArray<2, float> Array;

	typedef use_template_list<Accumulators, float,
                              mean_from_sum_calc,
                              acc::Moment2_2, acc::Variance,
                              acc::Skewness, acc::Kurtosis>
        accumulators_mean_sum;

	typedef use_template_list<Accumulators, float, acc::Variance>
        accumulators_var;

	typedef use_template_list<Accumulators, float, acc::Min, acc::Max>
        accumulators_minmax;

    accumulators_var test_acc;
    test_acc(1.2);
    test_acc(100.2);
    test_acc(1023);
    shouldEqual(detail::use<acc::Count>::val(test_acc), 3);
    shouldEqualTolerance(detail::use<acc::Mean>::val(test_acc), 374.799987792969, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(test_acc), 211715.125, 2e-15);
    
	unsigned h = 19;
	unsigned w = 10;
	unsigned max_label = 2;

	Array image(Array::size_type(h, w));
	Array labels(Array::size_type(h, w));

	for (unsigned i = 0; i < h; ++i)
	{
		for (unsigned j = 0; j < w; ++j)
		{
			if (i <= h / 2)
			{
				image(i, j) = 10 * i + j;
				labels(i, j) = 0;
			}
			else
			{
				image(i, j) = 1.234;
				labels(i, j) = 1;

			}
		}

	}
	ArrayOfRegionStatistics<accumulators_minmax, float> minmax(max_label - 1);
	ArrayOfRegionStatistics<accumulators_var, float>    sum_v(max_label - 1);
	ArrayOfRegionStatistics<accumulators_mean_sum, float>
                                                        sum_aors(max_label - 1);
	ArrayOfRegionStatistics<accumulators_colour_label<float>, float>
        colour_aors(max_label - 1);

	inspectTwoMultiArrays(srcMultiArrayRange(image),
                                 srcMultiArray(labels), minmax);
	inspectTwoMultiArrays(srcMultiArrayRange(image),
                                 srcMultiArray(labels), sum_aors);
	inspectTwoMultiArrays(srcMultiArrayRange(image),
                                 srcMultiArray(labels), sum_v);
	inspectTwoMultiArrays(srcMultiArrayRange(image),
                                 srcMultiArray(labels), colour_aors);

    shouldEqualTolerance(detail::use<acc::Min>::val(minmax[0]), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Max>::val(minmax[0]), 99, 2e-15);
    shouldEqual(detail::use<acc::Count>::val(sum_aors[0]), 100);
    shouldEqualTolerance(detail::use<acc::Mean>::val(sum_aors[0]), 49.5, 2e-15);
    shouldEqualTolerance(detail::use<acc::Moment2>::val(sum_aors[0]), 83324.9921875, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(sum_aors[0]), 833.249938964844, 2e-15);
    shouldEqual(detail::use<acc::Sum>::val(sum_aors[0]), 4950);
    shouldEqualTolerance(detail::use<mean_from_sum_calc>::val(sum_aors[0]), 49.5, 2e-15);
    shouldEqual(detail::use<acc::Moment2_2>::val(sum_aors[0]), 83325);
    shouldEqualTolerance(detail::use<acc::Skewness>::val(sum_aors[0]), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Kurtosis>::val(sum_aors[0]), 0.0179976038634777, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::val(sum_v[0]), 49.5, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(sum_v[0]), 833.249938964844, 2e-15);
    shouldEqual(detail::use<colour_label_acc>::val(colour_aors[0]).count(), 100);

    shouldEqualTolerance(detail::use<acc::Min>::val(minmax[1]), 1.23399996757507, 3e-15);
    shouldEqualTolerance(detail::use<acc::Max>::val(minmax[1]), 1.23399996757507, 3e-15);
    shouldEqual(detail::use<acc::Count>::val(sum_aors[1]), 90);
    shouldEqualTolerance(detail::use<acc::Mean>::val(sum_aors[1]), 1.23399996757507, 3e-15);
    shouldEqualTolerance(detail::use<acc::Moment2>::val(sum_aors[1]), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(sum_aors[1]), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Sum>::val(sum_aors[1]), 111.060066223145, 5e-15);
    shouldEqualTolerance(detail::use<mean_from_sum_calc>::val(sum_aors[1]), 1.23400068283081, 2e-15);
    shouldEqualTolerance(detail::use<acc::Moment2_2>::val(sum_aors[1]), 0, 2e-15);
    shouldEqualTolerance(detail::use<acc::Mean>::val(sum_v[1]), 1.23399996757507, 3e-15);
    shouldEqualTolerance(detail::use<acc::Variance>::val(sum_v[1]), 0, 2e-15);
    shouldEqual(detail::use<colour_label_acc>::val(colour_aors[1]).count(), 1);
}

void test5()
{
    vigra::ImageImportInfo import_info("of.gif");
    array_v2d test_image(shape_v2d(import_info.width(), import_info.height()));
    vigra::importImage(import_info, destImage(test_image));

    accumulators_colour_label<vpixel_type> colour_labels;
    inspectMultiArray(srcMultiArrayRange(test_image), colour_labels);
    shouldEqual(detail::use<colour_label_acc>::val(colour_labels).count(), 12);

    labels_v2d test_labels(shape_v2d(import_info.width(), import_info.height()));
    vigra::transformMultiArray(srcMultiArrayRange(test_image),
                               destMultiArray(test_labels),
                               detail::use<colour_label_acc>::val(colour_labels));
    shouldEqual(detail::use<colour_label_acc>::val(colour_labels).count(), 12);
    unsigned c_count = detail::use<colour_label_acc>::val(colour_labels).count();
    vigra::exportImage(srcImageRange(test_labels),
                       vigra::ImageExportInfo("of_labels.gif"));

    array_v2d test_image_gs(test_image);
    vigra::gaussianSmoothMultiArray(vigra::srcMultiArrayRange(test_image),
                                    vigra::destMultiArray(test_image_gs),
                                    options_v2d().stdDev(3, 5));

    using namespace vigra::functor;
    vigra::combineThreeMultiArrays(srcMultiArrayRange(test_image),
                                   srcMultiArray(test_image_gs),
                                   srcMultiArray(test_labels),
                                   destMultiArray(test_image),
                                   ifThenElse(!(Arg3() % Param(4)),
                                              Arg1(), Arg2()));

    vigra::exportImage(srcImageRange(test_image),
                       vigra::ImageExportInfo("of_gs.gif"));


	typedef use_template_list<Accumulators, vpixel_type,
                                  acc::Count,
                                  acc::Mean,
                                  acc::Variance,
                                  acc::Skewness, acc::Kurtosis,
                                  acc::Min, acc::Max
                             >
        accumulators_v2d;

	vigra::ArrayOfRegionStatistics<accumulators_v2d> test_aors(c_count);

    shouldEqual(accumulators_v2d::max_passes, 2);
    shouldEqual(acc::Moment2_2<vpixel_type>::number_of_passes, 2);
    shouldEqual((type_lists::max_value<detail::passes_number, unsigned, accumulators_v2d::list_type>::value), 2);
    
    shouldEqual(vigra::ArrayOfRegionStatistics<accumulators_v2d>::max_passes, 2);

    inspectTwoMultiArrays(srcMultiArrayRange(test_image),
                                 srcMultiArray(test_labels), test_aors);

	for (unsigned k = 0; k != c_count; ++k)
    {
        {
            double x_tab[] =
            {
                44251,
                467,
                4716,
                2061,
                1321,
                491,
                2331,
                2531,
                1386,
                1935,
                1209,
                1301
            };
            double x = x_tab[(k)];
            shouldEqual(nan_squash(detail::use<acc::Count>::val(test_aors[(k)])), x);
        }
        {
            double x_tab[][3] =
            {
                { 251, 253, 250 },
                { 190.07965721932, 209.987419616725, 181.582565021942 },
                { 67.913326469405, 201.435784493757, 156.365797422477 },
                { 241.492757985878, 193.318654606482, 120.5791435498 },
                { 187, 39, 36 },
                { 165.729286754482, 163.820392363662, 228.224842303106 },
                { 229.136746492872, 194.131506079963, 233.688057454825 },
                { 226.464699878818, 143.660482209742, 224.56605401776 },
                { 47, 43, 27 },
                { 87.5563448435715, 206.046916072375, 86.9893029678825 },
                { 140.884544460222, 62.4018669461537, 72.2334115190627 },
                { 77.6163845348157, 154.276412827855, 87.4082836188324 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Mean>::val(test_aors[(k)])).begin(), 5e-15);;
        }
        {
            double x_tab[][3] =
            {
                { 0, 0, 0 },
                { 151357.879015652, 53205.9839771735, 126078.50459435 },
                { 7027068.56229722, 395452.71116911, 1423540.62712511 },
                { 1095784.88644502, 378691.884028378, 1775553.3384559 },
                { 0, 0, 0 },
                { 385703.899603018, 426965.92005703, 26580.7561779026 },
                { 284311.862464607, 737198.877904507, 925147.288415809 },
                { 686241.437583964, 1421453.13838044, 2126896.11300357 },
                { 0, 0, 0 },
                { 1817091.39576743, 148856.520141535, 1785720.47479914 },
                { 814282.127559123, 2132526.15661061, 2264676.62617193 },
                { 1586333.78340258, 526576.835171952, 1421048.36393696 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Moment2>::val(test_aors[(k)])).begin(), 5e-15);;
        }
        {
            double x_tab[][3] =
            {
                { 0, 0, 0 },
                { 151357.879015652, 53205.9839771733, 126078.50459435 },
                { 7027068.56229725, 395452.711169113, 1423540.6271251 },
                { 1095784.88644502, 378691.884028384, 1775553.33845591 },
                { 0, 0, 0 },
                { 385703.899603019, 426965.920057029, 26580.7561779027 },
                { 284311.86246461, 737198.877904507, 925147.288415813 },
                { 686241.43758397, 1421453.13838044, 2126896.11300356 },
                { 0, 0, 0 },
                { 1817091.39576743, 148856.520141536, 1785720.47479914 },
                { 814282.127559124, 2132526.1566106, 2264676.62617193 },
                { 1586333.78340258, 526576.83517195, 1421048.36393696 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Moment2_2>::val(test_aors[(k)])).begin(), 5e-15);;
        }

        {
            double x_tab[][3] =
            {
                { 0, 0, 0 },
                { 324.106807314029, 113.931443205939, 269.975384570342 },
                { 1490.04846528779, 83.8534162784373, 301.853398457402 },
                { 531.67631559681, 183.741816607656, 861.500892021301 },
                { 0, 0, 0 },
                { 785.547657032624, 869.584358568289, 54.1359596291296 },
                { 121.969910967227, 316.258634879668, 396.888583618966 },
                { 271.134507144988, 561.617202046794, 840.338250890387 },
                { 0, 0, 0 },
                { 939.06532081004, 76.9284341816721, 922.852958552526 },
                { 673.517061670077, 1763.8760600584, 1873.18165936471 },
                { 1219.31881891052, 404.747759548003, 1092.2739154012 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(get<acc::Variance>(const_cast<const accumulators_v2d &>(test_aors[(k)]))).begin(), 5e-15);;
        }

        {
            double x_tab[][3] =
            {
                { 0, 0, 0 },
                { 324.106807314029, 113.931443205939, 269.975384570342 },
                { 1490.04846528779, 83.8534162784373, 301.853398457402 },
                { 531.67631559681, 183.741816607656, 861.500892021301 },
                { 0, 0, 0 },
                { 785.547657032624, 869.584358568289, 54.1359596291296 },
                { 121.969910967227, 316.258634879668, 396.888583618966 },
                { 271.134507144988, 561.617202046794, 840.338250890387 },
                { 0, 0, 0 },
                { 939.06532081004, 76.9284341816721, 922.852958552526 },
                { 673.517061670077, 1763.8760600584, 1873.18165936471 },
                { 1219.31881891052, 404.747759548003, 1092.2739154012 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Variance>::val(test_aors[(k)])).begin(), 5e-15);;
        }
        {
            double x_tab[][3] =
            {
                { 0, -0, -0 },
                { -0.0471999476074436, 0.0225100820944068, 0.0237907216783446 },
                { 0.0203748046142414, 0.0194368293242087, 0.0187743028374562 },
                { -0.0568030216004321, 0.0393764466733232, 0.0385930655364708 },
                { 0, -0, -0 },
                { 0.0129160274209086, 0.0125150661442668, -0.0118529891497778 },
                { -0.0941694199004041, -0.0546479844548244, -0.097100401212693 },
                { -0.0872381883732133, -0.0259197775770145, -0.0883967204365516 },
                { 0, -0, -0 },
                { 0.0251091853946739, 0.0256996935947648, 0.0254673230130096 },
                { 0.0198939243150143, 0.0251674700870824, 0.0188723958474796 },
                { 0.0145664507906367, 0.0122393102871076, 0.0124156595502772 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Skewness>::val(test_aors[(k)])).begin(), 5e-15);;
        }
        {
            double x_tab[][3] =
            {
                { 0, -0, -0 },
                { 0.0130922311877067, 0.00542906726593727, 0.0057287568787488 },
                { 0.000995372849455257, 0.000992068018240971, 0.00108301695110664 },
                { 0.0043777958589122, 0.00286214787763822, 0.00284567786675031 },
                { 0, -0, -0 },
                { 0.00473657112866356, 0.00471809290711345, 0.0116046931055875 },
                { 0.0114994795412696, 0.00647975768808986, 0.0113138535470433 },
                { 0.00974007485620935, 0.00343407472637769, 0.0098084392352915 },
                { 0, -0, -0 },
                { 0.00152394272486234, 0.00160790709278664, 0.00154341215192386 },
                { 0.00211927272212755, 0.00240580104758013, 0.00206332315300248 },
                { 0.00201510610202909, 0.00194267614266703, 0.00195582295690013 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(detail::use<acc::Kurtosis>::val(test_aors[(k)])).begin(), 5e-15);;
        }
         
        {
            double x_tab[] =
            {
                44251,
                467,
                4716,
                2061,
                1321,
                491,
                2331,
                2531,
                1386,
                1935,
                1209,
                1301
            };
            double x = x_tab[(k)];
            shouldEqual(nan_squash(get<acc::Count>(test_aors[(k)])), x);
        }

        {
            double x_tab[][3] =
            {
                { 0, 0, 0 },
                { 324.106807314029, 113.931443205939, 269.975384570342 },
                { 1490.04846528779, 83.8534162784373, 301.853398457402 },
                { 531.67631559681, 183.741816607656, 861.500892021301 },
                { 0, 0, 0 },
                { 785.547657032624, 869.584358568289, 54.1359596291296 },
                { 121.969910967227, 316.258634879668, 396.888583618966 },
                { 271.134507144988, 561.617202046794, 840.338250890387 },
                { 0, 0, 0 },
                { 939.06532081004, 76.9284341816721, 922.852958552526 },
                { 673.517061670077, 1763.8760600584, 1873.18165936471 },
                { 1219.31881891052, 404.747759548003, 1092.2739154012 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(get<acc::Variance>(test_aors[(k)])).begin(), 5e-15);;
        }

        {
            double x_tab[][3] =
            {
                { 251, 253, 250 },
                { 90.6309234812139, 208.945350778528, 170.528369020468 },
                { 36, 195, 145 },
                { 113.839833780299, 210.580726983075, 109.144489317492 },
                { 187, 39, 36 },
                { 118.593487117183, 114.212450351746, 216.499556981456 },
                { 134.229093937077, 53.3957699485034, 60.9830225692155 },
                { 110.446729613724, 16.4310826625539, 20.8175998381671 },
                { 47, 43, 27 },
                { 62.5690639680764, 199.390452546177, 63.5103920170893 },
                { 109.854720662883, 16.2678275526212, 19.7875100817635 },
                { 22.9135458480419, 122.145619928757, 34.6354994660794 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(get<acc::Min>(test_aors[(k)])).begin(), 5e-15);;
        }
        {
            double x_tab[][3] =
            {
                { 251, 253, 250 },
                { 231.339611982162, 243.60624313095, 234.452271513179 },
                { 242.654567477069, 246.121415567509, 247.323393948054 },
                { 251.997664863313, 183.163459568119, 97.3572759131751 },
                { 187, 39, 36 },
                { 238.105293397333, 240.32538730135, 246.48304522072 },
                { 241.08146673097, 205.096154262385, 242.043304857592 },
                { 239.323715428994, 190.259623102833, 239.897336643791 },
                { 47, 43, 27 },
                { 189.353806582618, 235.869789293751, 188.672697989038 },
                { 220.890291212331, 190.023294397645, 208.934745452046 },
                { 197.472899093947, 222.2912249782, 199.458474443288 }
            };
            TinyVector<double, 3> x;
            x = tab_assign<double, 3>(x, x_tab[(k)]);
            shouldEqualSequenceTolerance(x.begin(), x.end(), nan_squash(get<acc::Max>(test_aors[(k)])).begin(), 5e-15);;
        }
    }

}

struct ObjectFeaturesTest1
{
    void run1() { test1(); }
    void run2() { test2(); }
    void run3() { test3(); }
    void run4() { test4(); }
    void run5() { test5(); }
};

struct ObjectFeaturesTestSuite : public vigra::test_suite
{
    ObjectFeaturesTestSuite()
        : vigra::test_suite("ObjectFeaturesTestSuite")
    {
        add(testCase(&ObjectFeaturesTest1::run1));
        add(testCase(&ObjectFeaturesTest1::run2));
        add(testCase(&ObjectFeaturesTest1::run3));
        add(testCase(&ObjectFeaturesTest1::run4));
        add(testCase(&ObjectFeaturesTest1::run5));
    }
};


int main(int argc, char** argv)
{
    ObjectFeaturesTestSuite test;
    const int failed = test.run(vigra::testsToBeExecuted(argc, argv));
    std::cout << test.report() << std::endl;

    return failed != 0;
}
