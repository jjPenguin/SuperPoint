#include <iostream>
#include <cstdio>
#include <vector>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <numpy/ndarrayobject.h>
#include "boost/tuple/tuple.hpp"
#include "boost/python/object.hpp"
#include <boost/tuple/tuple_comparison.hpp>
#include <limits>
#include <map>
#include "connected_components.cpp"
#include "random_subgraph.cpp"

namespace bp = boost::python;
namespace ei = Eigen;
namespace bpn = boost::python::numpy;

typedef ei::Matrix<float, 3, 3> Matrix3f;
typedef ei::Matrix<float, 3, 1> Vector3f;

typedef boost::tuple< std::vector< std::vector<float> >, std::vector< std::vector<uint8_t> >, std::vector< std::vector<uint32_t> >, std::vector<std::vector<uint32_t> > > Custom_tuple;
typedef boost::tuple< std::vector< std::vector<uint32_t> >, std::vector<uint32_t> > Components_tuple;
typedef boost::tuple< std::vector<uint8_t>, std::vector<uint8_t> > Subgraph_tuple;

typedef boost::tuple< uint32_t, uint32_t, uint32_t > Space_tuple;

struct VecToArray
{//converts a vector<uint8_t> to a numpy array
    static PyObject * convert(const std::vector<uint8_t> & vec) {
    npy_intp dims = vec.size();
    PyObject * obj = PyArray_SimpleNew(1, &dims, NPY_UINT8);
    void * arr_data = PyArray_DATA((PyArrayObject*)obj);
    memcpy(arr_data, &vec[0], dims * sizeof(uint8_t));
    return obj;
    }
};

template <class T>
struct VecvecToArray
{//converts a vector< vector<uint32_t> > to a numpy 2d array
    static PyObject * convert(const std::vector< std::vector<T> > & vecvec)
    {
        npy_intp dims[2];
        dims[0] = vecvec.size();
        dims[1] = vecvec[0].size();
        PyObject * obj;
        if (typeid(T) == typeid(uint8_t))
            obj = PyArray_SimpleNew(2, dims, NPY_UINT8);
        else if (typeid(T) == typeid(float))
            obj = PyArray_SimpleNew(2, dims, NPY_FLOAT32);
        else if (typeid(T) == typeid(uint32_t))
            obj = PyArray_SimpleNew(2, dims, NPY_UINT32);
        void * arr_data = PyArray_DATA((PyArrayObject*)obj);
        std::size_t cell_size = sizeof(T);
        for (std::size_t i = 0; i < dims[0]; i++)
        {
            memcpy(arr_data + i * dims[1] * cell_size, &(vecvec[i][0]), dims[1] * cell_size);
        }
        return obj;
    }
};

struct VecToArray32
{//converts a vector<uint32_t> to a numpy array
    static PyObject * convert(const std::vector<uint32_t> & vec)
    {
        npy_intp dims = vec.size();
        PyObject * obj = PyArray_SimpleNew(1, &dims, NPY_UINT32);
        void * arr_data = PyArray_DATA((PyArrayObject*)obj);
        memcpy(arr_data, &vec[0], dims * sizeof(uint32_t));
        return obj;
    }
};


template<class T>
struct VecvecToList
{//converts a vector< vector<T> > to a list
        static PyObject* convert(const std::vector< std::vector<T> > & vecvec)
    {
        boost::python::list* pylistlist = new boost::python::list();
        for(size_t i = 0; i < vecvec.size(); i++)
        {
            boost::python::list* pylist = new boost::python::list();
            for(size_t j = 0; j < vecvec[i].size(); j++)
            {
                pylist->append(vecvec[i][j]);
            }
            pylistlist->append((pylist, pylist[0]));
        }
        return pylistlist->ptr();
    }
};

struct to_py_tuple
{//converts to a python tuple
    static PyObject* convert(const Custom_tuple & c_tuple){
        bp::list values;

        PyObject * pyo1 = VecvecToArray<float>::convert(c_tuple.get<0>());
        PyObject * pyo2 = VecvecToArray<uint8_t>::convert(c_tuple.get<1>());
        PyObject * pyo3 = VecvecToArray<uint32_t>::convert(c_tuple.get<2>());
        PyObject * pyo4 = VecvecToArray<uint32_t>::convert(c_tuple.get<3>());

        values.append(bp::handle<>(bp::borrowed(pyo1)));
        values.append(bp::handle<>(bp::borrowed(pyo2)));
        values.append(bp::handle<>(bp::borrowed(pyo3)));
        values.append(bp::handle<>(bp::borrowed(pyo4)));

        return bp::incref( bp::tuple( values ).ptr() );
    }
};

struct to_py_tuple_components
{//converts output to a python tuple
    static PyObject* convert(const Components_tuple& c_tuple){
        bp::list values;
        //add all c_tuple items to "values" list

        PyObject * vecvec_pyo = VecvecToList<uint32_t>::convert(c_tuple.get<0>());
        PyObject * vec_pyo = VecToArray32::convert(c_tuple.get<1>());

        values.append(bp::handle<>(bp::borrowed(vecvec_pyo)));
        values.append(bp::handle<>(bp::borrowed(vec_pyo)));

        return bp::incref( bp::tuple( values ).ptr() );
    }
};

struct to_py_tuple_subgraph
{//converts output to a python tuple
    static PyObject* convert(const Subgraph_tuple& s_tuple){
        bp::list values;
        //add all c_tuple items to "values" list

        PyObject * vec_pyo1 = VecToArray::convert(s_tuple.get<0>());
        PyObject * vec_pyo2 = VecToArray::convert(s_tuple.get<1>());

        values.append(bp::handle<>(bp::borrowed(vec_pyo1)));
        values.append(bp::handle<>(bp::borrowed(vec_pyo2)));

        return bp::incref( bp::tuple( values ).ptr() );
    }
};

class AttributeGrid {
//voxelization of the space, allows to accumulate the position, the color and labels
    std::map<Space_tuple, uint64_t> space_tuple_to_index;//associate eeach non-empty voxel to an index
    uint64_t index;
    std::vector<uint32_t> bin_count;//count the number of point in each non-empty voxel
    std::vector< std::vector<float> > acc_xyz;//accumulate the position of the points
    std::vector< std::vector<uint32_t> > acc_rgb;//accumulate the color of the points
    std::vector< std::vector<uint32_t> > acc_labels;//accumulate the label of the points
    std::vector< std::vector<uint32_t> > acc_objects;//accumulate the object indices of the points
  public:
    AttributeGrid():
       index(0)
    {}
    //---methods for the occurence grid---
    uint64_t n_nonempty_voxels()
    {
        return this->index;
    }
    uint32_t get_index(uint32_t x_bin, uint32_t  y_bin, uint32_t z_bin)
    {
        return space_tuple_to_index.at(Space_tuple(x_bin, y_bin, z_bin));
    }
    bool add_occurence(uint32_t x_bin, uint32_t  y_bin, uint32_t z_bin)
    {
        Space_tuple st(x_bin, y_bin, z_bin);
        auto inserted = space_tuple_to_index.insert(std::pair<Space_tuple, uint64_t>(st, index));
        if (inserted.second)
        {
            this->index++;
            return true;
        }
        else
        {
            return false;
        }
    }
    std::map<Space_tuple,uint64_t>::iterator begin()
    {
        return this->space_tuple_to_index.begin();
    }
    std::map<Space_tuple,uint64_t>::iterator end()
    {
        return this->space_tuple_to_index.end();
    }
    //---methods for accumulating atributes---
    void initialize(uint8_t n_labels, int n_objects)
    {//must be run once space_tuple_to_index is complete and the number of non-empty voxels is known
        bin_count  = std::vector<uint32_t>(this->index, 0);
        acc_xyz    = std::vector< std::vector<float> >(this->index, std::vector <float>(3,0));
        acc_rgb    = std::vector< std::vector<uint32_t> >(this->index, std::vector <uint32_t>(3,0));
        acc_labels = std::vector< std::vector<uint32_t> >(this->index, std::vector <uint32_t>(n_labels+1,0));
        acc_objects = std::vector< std::vector<uint32_t> >(this->index, std::vector <uint32_t>(n_objects+1,0));
    }
    uint32_t get_count(uint64_t voxel_index)
    {
        return bin_count.at(voxel_index);
    }
    std::vector<float> get_pos(uint64_t voxel_index)
    {
        return acc_xyz.at(voxel_index);
    }
    std::vector<uint32_t> get_rgb(uint64_t voxel_index)
    {
        return acc_rgb.at(voxel_index);
    }
    std::vector<uint32_t> get_acc_labels(uint64_t voxel_index)
    {
        return acc_labels.at(voxel_index);
    }
    std::vector<uint32_t> get_acc_objects(uint64_t voxel_index)
    {
        return acc_objects.at(voxel_index);
    }
    uint8_t get_label(uint64_t voxel_index)
    {//return the majority label from this voxel
     //ignore the unlabeled points (0), unless all points are unlabeled
        std::vector<uint32_t> label_hist = acc_labels.at(voxel_index);
        std::vector<uint32_t>::iterator chosen_label = std::max_element(label_hist.begin() + 1, label_hist.end());
        if (*chosen_label == 0)
        {
            return 0;
        }
        else
        {
            return (uint8_t)std::distance(label_hist.begin(), chosen_label);
        }
    }
    uint32_t get_object(uint64_t voxel_index)
    {//return the majority object from this voxel
     //ignore the unattributed points (0), unless all points are unattributed
        std::vector<uint32_t> object_hist = acc_objects.at(voxel_index);
        std::vector<uint32_t>::iterator chosen_object = std::max_element(object_hist.begin() + 1, object_hist.end());
        if (*chosen_object == 0)
        {
            return 0;
        }
        else
        {
            return (uint32_t)std::distance(object_hist.begin(), chosen_object);
        }
    }
    void add_attribute(uint32_t x_bin, uint32_t  y_bin, uint32_t z_bin, float x, float y, float z, uint8_t r, uint8_t g, uint8_t b)
    {//add a point x y z in voxel x_bin y_bin z_bin
        uint64_t bin = get_index(x_bin, y_bin, z_bin);
        bin_count.at(bin) = bin_count.at(bin) + 1;
        acc_xyz.at(bin).at(0) = acc_xyz.at(bin).at(0) + x;
        acc_xyz.at(bin).at(1) = acc_xyz.at(bin).at(1) + y;
        acc_xyz.at(bin).at(2) = acc_xyz.at(bin).at(2) + z;
        acc_rgb.at(bin).at(0) = acc_rgb.at(bin).at(0) + r;
        acc_rgb.at(bin).at(1) = acc_rgb.at(bin).at(1) + g;
        acc_rgb.at(bin).at(2) = acc_rgb.at(bin).at(2) + b;
    }
    void add_attribute(uint32_t x_bin, uint32_t  y_bin, uint32_t z_bin, float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t label)
    {//add a point x y z in voxel x_bin y_bin z_bin - with label
        uint32_t bin =get_index(x_bin, y_bin, z_bin);
        bin_count.at(bin) = bin_count.at(bin) + 1;
        acc_xyz.at(bin).at(0) = acc_xyz.at(bin).at(0) + x;
        acc_xyz.at(bin).at(1) = acc_xyz.at(bin).at(1) + y;
        acc_xyz.at(bin).at(2) = acc_xyz.at(bin).at(2) + z;
        acc_rgb.at(bin).at(0) = acc_rgb.at(bin).at(0) + r;
        acc_rgb.at(bin).at(1) = acc_rgb.at(bin).at(1) + g;
        acc_rgb.at(bin).at(2) = acc_rgb.at(bin).at(2) + b;
        acc_labels.at(bin).at(label) = acc_labels.at(bin).at(label) + 1;
    }
    void add_attribute(uint32_t x_bin, uint32_t  y_bin, uint32_t z_bin, float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t label, uint32_t object)
    {//add a point x y z in voxel x_bin y_bin z_bin - with label
        uint32_t bin =get_index(x_bin, y_bin, z_bin);
        bin_count.at(bin) = bin_count.at(bin) + 1;
        acc_xyz.at(bin).at(0) = acc_xyz.at(bin).at(0) + x;
        acc_xyz.at(bin).at(1) = acc_xyz.at(bin).at(1) + y;
        acc_xyz.at(bin).at(2) = acc_xyz.at(bin).at(2) + z;
        acc_rgb.at(bin).at(0) = acc_rgb.at(bin).at(0) + r;
        acc_rgb.at(bin).at(1) = acc_rgb.at(bin).at(1) + g;
        acc_rgb.at(bin).at(2) = acc_rgb.at(bin).at(2) + b;
        acc_labels.at(bin).at(label) = acc_labels.at(bin).at(label) + 1;
        acc_objects.at(bin).at(object) = acc_objects.at(bin).at(object) + 1;
    }
};

PyObject *  prune(const bpn::ndarray & xyz ,float voxel_size, const bpn::ndarray & rgb, const bpn::ndarray & labels, const bpn::ndarray & objects, const int n_labels, const int n_objects)
{//prune the point cloud xyz with a regular voxel grid
    std::cout << "=========================" << std::endl;
    std::cout << "======== pruning ========" << std::endl;
    std::cout << "=========================" << std::endl;
    uint64_t n_ver = bp::len(xyz);
    bool have_labels = n_labels>0;
    bool have_objects = n_objects>0;
    //---read the numpy arrays data---
    const float * xyz_data = reinterpret_cast<float*>(xyz.get_data());          
    const uint8_t * rgb_data = reinterpret_cast<uint8_t*>(rgb.get_data());      
    const uint8_t * label_data;
    if (have_labels)
        label_data = reinterpret_cast<uint8_t*>(labels.get_data());
    const uint32_t * object_data;
    if (have_objects)
        object_data = reinterpret_cast<uint32_t*>(objects.get_data());
    //---find min max of xyz----
    float x_max = std::numeric_limits<float>::lowest(), x_min = std::numeric_limits<float>::max();
    float y_max = std::numeric_limits<float>::lowest(), y_min = std::numeric_limits<float>::max();
    float z_max = std::numeric_limits<float>::lowest(), z_min = std::numeric_limits<float>::max();

    #pragma omp parallel for reduction(max : x_max, y_max, z_max), reduction(min : x_min, y_min, z_min)
    for (std::size_t i_ver = 0; i_ver < n_ver; i_ver ++)
    {
        if (x_max < xyz_data[3 * i_ver]){           x_max = xyz_data[3 * i_ver];}
        if (y_max < xyz_data[3 * i_ver + 1]){       y_max = xyz_data[3 * i_ver + 1];}
        if (z_max < xyz_data[3 * i_ver + 2]){       z_max = xyz_data[3 * i_ver + 2];}
        if (x_min > xyz_data[3 * i_ver]){           x_min = xyz_data[3 * i_ver];}
        if (y_min > xyz_data[3 * i_ver + 1]){       y_min = xyz_data[3 * i_ver + 1];}
        if (z_min > xyz_data[3 * i_ver + 2 ]){      z_min = xyz_data[3 * i_ver + 2];}
    }
    //---compute the voxel grid size---
    uint32_t n_bin_x = std::ceil((x_max - x_min) / voxel_size);
    uint32_t n_bin_y = std::ceil((y_max - y_min) / voxel_size);
    uint32_t n_bin_z = std::ceil((z_max - z_min) / voxel_size);
    std::cout << "Voxelization into " << n_bin_x << " x " << n_bin_y << " x " << n_bin_z << " grid" << std::endl;
    //---detect non-empty voxels----
    AttributeGrid vox_grid;
    for (std::size_t i_ver = 0; i_ver < n_ver; i_ver ++)
    {
        uint32_t bin_x = std::floor((xyz_data[3 * i_ver] - x_min) / voxel_size);
        uint32_t bin_y = std::floor((xyz_data[3 * i_ver + 1] - y_min) / voxel_size);
        uint32_t bin_z = std::floor((xyz_data[3 * i_ver + 2] - z_min) / voxel_size);
        vox_grid.add_occurence(bin_x, bin_y, bin_z);
    }
    std::cout << "Reduced from " << n_ver << " to " << vox_grid.n_nonempty_voxels() << " points ("
              << std::ceil(10000 * vox_grid.n_nonempty_voxels() / n_ver)/100 << "%)" << std::endl;
    vox_grid.initialize(n_labels, n_objects);
    //---accumulate points in the voxel map----
    for (std::size_t i_ver = 0; i_ver < n_ver; i_ver ++)
    {
        uint32_t bin_x = std::floor((xyz_data[3 * i_ver    ] - x_min) / voxel_size);
        uint32_t bin_y = std::floor((xyz_data[3 * i_ver + 1] - y_min) / voxel_size);
        uint32_t bin_z = std::floor((xyz_data[3 * i_ver + 2] - z_min) / voxel_size);
        if (have_labels&&!have_objects)
            vox_grid.add_attribute(bin_x, bin_y, bin_z
                    , xyz_data[3 * i_ver], xyz_data[3 * i_ver + 1], xyz_data[3 * i_ver + 2]
                    , rgb_data[3 * i_ver], rgb_data[3 * i_ver + 1], rgb_data[3 * i_ver + 2], label_data[i_ver]);
        else if(have_labels&&have_objects)
            vox_grid.add_attribute(bin_x, bin_y, bin_z
                    , xyz_data[3 * i_ver], xyz_data[3 * i_ver + 1], xyz_data[3 * i_ver + 2]
                    , rgb_data[3 * i_ver], rgb_data[3 * i_ver + 1], rgb_data[3 * i_ver + 2], label_data[i_ver], object_data[i_ver]);
        else
            vox_grid.add_attribute(bin_x, bin_y, bin_z
                    , xyz_data[3 * i_ver], xyz_data[3 * i_ver + 1], xyz_data[3 * i_ver + 2]
                    , rgb_data[3 * i_ver], rgb_data[3 * i_ver + 1], rgb_data[3 * i_ver + 2]);
    }
    //---compute pruned cloud----
    std::vector< std::vector< float > > pruned_xyz(vox_grid.n_nonempty_voxels(), std::vector< float >(3, 0.f));
    std::vector< std::vector< uint8_t > > pruned_rgb(vox_grid.n_nonempty_voxels(), std::vector< uint8_t >(3, 0));
    std::vector< std::vector< uint32_t > > pruned_labels(vox_grid.n_nonempty_voxels(), std::vector< uint32_t >(n_labels + 1, 0));
    std::vector< std::vector< uint32_t > > pruned_objects(vox_grid.n_nonempty_voxels(), std::vector< uint32_t >(n_objects + 1, 0));
    for (std::map<Space_tuple,uint64_t>::iterator it_vox=vox_grid.begin(); it_vox!=vox_grid.end(); ++it_vox)
    {//loop over the non-empty voxels and compute the average posiition/color + majority label
        uint64_t voxel_index = it_vox->second; //
        float count = (float)vox_grid.get_count(voxel_index);
        std::vector<float> pos = vox_grid.get_pos(voxel_index);
        pos.at(0) = pos.at(0) / count;
        pos.at(1) = pos.at(1) / count;
        pos.at(2) = pos.at(2) / count;
        pruned_xyz.at(voxel_index) = pos;
        std::vector<uint32_t> col = vox_grid.get_rgb(voxel_index);
        std::vector<uint8_t> col_uint8_t(3);
        col_uint8_t.at(0) = (uint8_t)((float) col.at(0) / count);
        col_uint8_t.at(1) = (uint8_t)((float) col.at(1) / count);
        col_uint8_t.at(2) = (uint8_t)((float) col.at(2) / count);
        pruned_rgb.at(voxel_index) = col_uint8_t;
        pruned_labels.at(voxel_index) = vox_grid.get_acc_labels(voxel_index);
        pruned_objects.at(voxel_index) = vox_grid.get_acc_objects(voxel_index);
    }
    return to_py_tuple::convert(Custom_tuple(pruned_xyz,pruned_rgb, pruned_labels, pruned_objects));
}



PyObject * compute_geof(const bpn::ndarray & xyz ,const bpn::ndarray & target, int k_nn)
{//compute the following geometric features (geof) features of a point cloud:
 //linearity planarity scattering verticality
    std::size_t n_ver = bp::len(xyz);                                                       // xyz의 길이
    std::vector< std::vector< float > > geof(n_ver, std::vector< float >(4,0));             // geof 4 x len(xyz) 만큼의 배열
    //--- read numpy array data---
    const uint32_t * target_data = reinterpret_cast<uint32_t*>(target.get_data());          // 
    const float * xyz_data = reinterpret_cast<float*>(xyz.get_data());
    print(xyz_data)
    std::size_t s_ver = 0;
    #pragma omp parallel for schedule(static)                                               // 지정된 스레드에 맞춰 스레드 생성
    for (std::size_t i_ver = 0; i_ver < n_ver; i_ver++)                                     // 점 A와 그 주변 점들(총 46개)을 하나의 position 행렬로 만든다.
    {//each point can be treated in parallell 
    
        //--- compute 3d covariance matrix of neighborhood ---
        ei::MatrixXf position(k_nn+1,3);                        // 46 x 3 매트릭스 생성 
        std::size_t i_edg = k_nn * i_ver;                       // 45 * i_ver 개씩  
        std::size_t ind_nei;                                    
        position(0,0) = xyz_data[3 * i_ver];                    // 점 A의 x 데이터 
        position(0,1) = xyz_data[3 * i_ver + 1];                // y 데이터 
        position(0,2) = xyz_data[3 * i_ver + 2];                // z 데이터 
        for (std::size_t i_nei = 0; i_nei < k_nn; i_nei++)      // 
        {
                //add the neighbors to the position matrix
            ind_nei = target_data[i_edg];                       // 점 A에 이웃한 점들의 위치 45 * i_ver 
            position(i_nei+1,0) = xyz_data[3 * ind_nei];        
            position(i_nei+1,1) = xyz_data[3 * ind_nei + 1];
            position(i_nei+1,2) = xyz_data[3 * ind_nei + 2];
            i_edg++;
        }
        // PCA 주성분 분석, 데이터들을 정사영 시켜 차원을 낮춘다면 어떤 벡터에 데이터들을 정사영 시켜야 원래의 데이터 구조를 잘 유지할 수 있을까?
        // 이 때 고유 벡터를 사용한다. 고유 벡터는 방향은 바뀌지 않고, 크기만 변환시킴
        // 고유 vector(eigenvector)의 의미를 잘 생각해보면, 
        // 고유 벡터는 그 행렬이 벡터에 작용하는 주축(principal axis)의 방향을 나타내므로 
        // 공분산 행렬의 고유 벡터는 데이터가 어떤 방향으로 분산되어 있는지를 나타내준다고 할 수 있다.

        // 고유 값은 고유벡터 방향으로 얼마만큼의 크기로 벡터공간이 늘려지는 지를 얘기한다. 
        // 따라서 고유 값이 큰 순서대로 고유 벡터를 정렬하면 결과적으로 중요한 순서대로 주성분을 구하는 것이 된다. 
        
        // compute the covariance matrix
        ei::MatrixXf centered_position = position.rowwise() - position.colwise().mean();                // 만든 데이터의 열들의 평균값을 0으로 만들기 위해 열들의 평균을 데이터 행렬에서 뺀다.
                                                                                                        // 이렇게 하는 이유는 데이터 분포의 중심을 중심축으로 움직이는 벡터를 찾는데 도움을 주기 때문이다.
        
        ei::Matrix3f cov = (centered_position.adjoint() * centered_position) / float(k_nn + 1);         // 분자 : 각 데이터 특징들이 얼마나 닮아 있는가를 의미, 분모 : 데이터의 샘플수 만큼 나누어 준다. 공분산 행렬
                                                                                                     
        ei::EigenSolver<Matrix3f> es(cov);                                                              // 공분산 행렬의 고유값과 고유벡터를 구할 수 있다.
                                                                                                        
        //--- compute the eigen values and vectors---
        std::vector<float> ev = {es.eigenvalues()[0].real(),es.eigenvalues()[1].real(),es.eigenvalues()[2].real()};         // 계산된 고유값을 벡터로
        std::vector<int> indices(3);        // 0으로 이루어진 3x1벡터
        std::size_t n(0);                   
        std::generate(std::begin(indices), std::end(indices), [&]{ return n++; });          // 벡터 indices에 0,1,2 들어간다.
        std::sort(std::begin(indices),std::end(indices),                                    // 고유 값이 큰 순서대로 고유벡터를 정렬하면 결과적으로 중요한 순서대로 주성분을 구하는 것이 된다.
                       [&](int i1, int i2) { return ev[i1] > ev[i2]; } );                   
        std::vector<float> lambda = {(std::max(ev[indices[0]],0.f)),                        // 람다는 고유 벡터를 통해 정사영했을 때의 분산을 의미함 랑그라주 승수법에 의해
                                    (std::max(ev[indices[1]],0.f)),                         
                                    (std::max(ev[indices[2]],0.f))};
        std::vector<float> v1 = {es.eigenvectors().col(indices[0])(0).real()                // 각 고유값에 해당하는 고유 벡터
                               , es.eigenvectors().col(indices[0])(1).real()                
                               , es.eigenvectors().col(indices[0])(2).real()};              
        std::vector<float> v2 = {es.eigenvectors().col(indices[1])(0).real()
                               , es.eigenvectors().col(indices[1])(1).real()
                               , es.eigenvectors().col(indices[1])(2).real()};
        std::vector<float> v3 = {es.eigenvectors().col(indices[2])(0).real()
                               , es.eigenvectors().col(indices[2])(1).real()
                               , es.eigenvectors().col(indices[2])(2).real()};
        // --- compute the dimensionality features---
        // 첫번째 주축 고유 값인 l0 ------->>l1=l2 일 때, linearity을 의미
        // l0 = l1 --------->> l2 일 때, 첫번째, 두번째가 같을 때, planarity을 의미
        // l0 = l1 = l2 일 떄, scatter 즉 Sphericity 구형 
        float linearity  = (sqrtf(lambda[0]) - sqrtf(lambda[1])) / sqrtf(lambda[0]);                        // l0 가 큰값인데, l1과 비교하여 얼마나 길쭉하게 되어 있는가                                                                                                        
        float planarity  = (sqrtf(lambda[1]) - sqrtf(lambda[2])) / sqrtf(lambda[0]);                        // l1과 l2를 비교하여 얼마나 평면하지
        float scattering =  sqrtf(lambda[2]) / sqrtf(lambda[0]);                                            // l3 비교하여 얼마나 퍼져있는지
        //--- compute the verticality feature---    
        std::vector<float> unary_vector =                                                       
            {lambda[0] * fabsf(v1[0]) + lambda[1] * fabsf(v2[0]) + lambda[2] * fabsf(v3[0])                 // 단항 벡터를 고유 값에 의해 가중치가 부여된 고유벡터 좌표의 절대 값 합계로 정의
            ,lambda[0] * fabsf(v1[1]) + lambda[1] * fabsf(v2[1]) + lambda[2] * fabsf(v3[1])                 // 단항 벡터의 vertical component는 이웃하는 점들의 verticality를 특징화한다.
            ,lambda[0] * fabsf(v1[2]) + lambda[1] * fabsf(v2[2]) + lambda[2] * fabsf(v3[2])};
        float norm = sqrt(unary_vector[0] * unary_vector[0] + unary_vector[1] * unary_vector[1]             // 단항 벡터의 길이를 구함
                        + unary_vector[2] * unary_vector[2])
        float verticality = unary_vector[2] / norm;                                                         // 크기로 나누어 수직인지 아닌지를 판별 
                                                                                                            // 수직인 경우 1, 평평한 경우 0, 비스듬히 누워 있는 경우 0.5라는 값을 가지게 된다.
        //---fill the geof vector---
        geof[i_ver][0] = linearity;
        geof[i_ver][1] = planarity;
        geof[i_ver][2] = scattering;
        geof[i_ver][3] = verticality;
        //---progression---
        s_ver++;//if run in parellel s_ver behavior is udnefined, but gives a good indication of progress
        if (s_ver % 10000 == 0)
        {
            std::cout << s_ver << "% done          \r" << std::flush;
            std::cout << ceil(s_ver*100/n_ver) << "% done          \r" << std::flush;
        }
    }
    std::cout <<  std::endl;
    return VecvecToArray<float>::convert(geof);
}


PyObject * connected_comp(const uint32_t n_ver, const bpn::ndarray & source, const bpn::ndarray & target, const bpn::ndarray & active_edg, const int cutoff)
{//read data and run the L0-cut pursuit partition algorithm
    const uint32_t n_edg = bp::len(source);
    const uint32_t * source_data = reinterpret_cast<uint32_t*>(source.get_data());
    const uint32_t * target_data = reinterpret_cast<uint32_t*>(target.get_data());
    const char * active_edg_data = reinterpret_cast<char*>(active_edg.get_data());

    std::vector<uint32_t> in_component(n_ver,0);
    std::vector< std::vector<uint32_t> > components(1,std::vector<uint32_t>());

    connected_components(n_ver, n_edg, source_data, target_data, active_edg_data, in_component, components, cutoff);

    return to_py_tuple_components::convert(Components_tuple(components, in_component));
}


PyObject * random_subgraph(const int n_ver, const bpn::ndarray & source, const bpn::ndarray & target, const int subgraph_size)
{//read data and run the L0-cut pursuit partition algorithm

    const int n_edg = bp::len(source);
    const uint32_t * source_data = reinterpret_cast<uint32_t*>(source.get_data());
    const uint32_t * target_data = reinterpret_cast<uint32_t*>(target.get_data());

    std::vector<uint8_t> selected_edges(n_edg,0);
    std::vector<uint8_t> selected_vertices(n_ver,0);

    subgraph::random_subgraph(n_ver, n_edg, source_data, target_data, subgraph_size, selected_edges.data(), selected_vertices.data());

    return to_py_tuple_subgraph::convert(Subgraph_tuple(selected_edges,selected_vertices));
}

using namespace boost::python;
BOOST_PYTHON_MODULE(libply_c)
{
    _import_array();
    bp::to_python_converter<std::vector<std::vector<float>, std::allocator<std::vector<float> > >, VecvecToArray<float> >();
    bp::to_python_converter< Custom_tuple, to_py_tuple>();
    Py_Initialize();
    bpn::initialize();
    def("compute_geof", compute_geof);
    def("prune", prune);
    def("connected_comp", connected_comp);
    def("random_subgraph", random_subgraph);
}
