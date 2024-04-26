#ifndef _DNNL_KERNELS_HPP_
#define _DNNL_KERNELS_HPP_

#include <algorithm>
#include <queue>

namespace vt { namespace dnnl_kernels {

template<typename T>
void fill_causal_mask(int* m, T* o, T minv, int full_tokens, int nt_end) {
    for ( int i = 0; i < full_tokens; i++) {
        o[i] = minv;
    }

    for ( int i = 0; i <= nt_end; i++) {
        if ( m[i] != 0 ) {
            o[i] = 0;
        }
    }
}

void binary_float(tensor_t a, tensor_t b, tensor_t c, dnnl::algorithm op ) {
    auto tag = dnnl::memory::format_tag::abcd;
    if ( a->shape().dim() == 3) {
        tag = dnnl::memory::format_tag::abc;
    }
    if ( a->shape().dim() == 2) {
        tag = dnnl::memory::format_tag::ab;
    }
    if ( a->shape().dim() == 1) {
        tag = dnnl::memory::format_tag::a;
    }

    auto amem_desc = a->dnnl_float()->build_memory_desc( a->shape().vec(),  tag);
    auto bmem_desc = b->dnnl_float()->build_memory_desc( b->shape().vec(),  tag);
    auto cmem_desc = c->dnnl_float()->build_memory_desc( c->shape().vec(),  tag);

    auto amem = a->dnnl_float()->build_memory(amem_desc);
    auto bmem = b->dnnl_float()->build_memory(bmem_desc);
    auto cmem = c->dnnl_float()->build_memory(cmem_desc);

    auto eng = *ComputingContext::dnnl_engine;
    auto stream = *ComputingContext::dnnl_stream;

#ifdef _DNNL_GPU_
    if ( a->dnnl_float()->is_gpu() ) {
        eng = *ComputingContext::dnnl_gpu_engine;
        stream = *ComputingContext::dnnl_gpu_stream;
    }
#endif

    auto binary_pd = dnnl::binary::primitive_desc(eng, op, amem_desc, bmem_desc, cmem_desc);
    auto binary_prim = dnnl::binary(binary_pd);

    std::unordered_map<int, dnnl::memory> binary_args;
    binary_args[DNNL_ARG_SRC_0] = amem;
    binary_args[DNNL_ARG_SRC_1] = bmem;
    binary_args[DNNL_ARG_DST] = cmem;

    binary_prim.execute(stream, binary_args);
}

void binary_fp16(tensor_t a, tensor_t b, tensor_t c, dnnl::algorithm op ) {
    auto tag = dnnl::memory::format_tag::abcd;
    if ( a->shape().dim() == 3) {
        tag = dnnl::memory::format_tag::abc;
    }
    if ( a->shape().dim() == 2) {
        tag = dnnl::memory::format_tag::ab;
    }
    if ( a->shape().dim() == 1) {
        tag = dnnl::memory::format_tag::a;
    }

    auto amem_desc = a->dnnl_fp16()->build_memory_desc( a->shape().vec(),  tag);
    auto bmem_desc = b->dnnl_fp16()->build_memory_desc( b->shape().vec(),  tag);
    auto cmem_desc = c->dnnl_fp16()->build_memory_desc( c->shape().vec(),  tag);

    auto amem = a->dnnl_fp16()->build_memory(amem_desc);
    auto bmem = b->dnnl_fp16()->build_memory(bmem_desc);
    auto cmem = c->dnnl_fp16()->build_memory(cmem_desc);

    auto eng = *ComputingContext::dnnl_engine;
    auto stream = *ComputingContext::dnnl_stream;

#ifdef _DNNL_GPU_
    if ( a->dnnl_fp16()->is_gpu() ) {
        eng = *ComputingContext::dnnl_gpu_engine;
        stream = *ComputingContext::dnnl_gpu_stream;
    }
#endif

    auto binary_pd = dnnl::binary::primitive_desc(eng, op, amem_desc, bmem_desc, cmem_desc);
    auto binary_prim = dnnl::binary(binary_pd);

    std::unordered_map<int, dnnl::memory> binary_args;
    binary_args[DNNL_ARG_SRC_0] = amem;
    binary_args[DNNL_ARG_SRC_1] = bmem;
    binary_args[DNNL_ARG_DST] = cmem;

    binary_prim.execute(stream, binary_args);
}

template<typename T>
void eltwise(T* in, T* out,  size_t items, ::dnnl::algorithm op, float alpha, float beta) {
    auto src_md = in->build_memory_desc( {items},  dnnl::memory::format_tag::a);
    auto dst_md = out->build_memory_desc( {items},  dnnl::memory::format_tag::a);
    auto src_mem = in->build_memory(src_md);
    auto dst_mem = out->build_memory(dst_md);

    std::unordered_map<int, dnnl::memory> eltwise_args;
    eltwise_args[DNNL_ARG_SRC] = src_mem;
    eltwise_args[DNNL_ARG_DST] = dst_mem;

#ifdef _DNNL_GPU_
    if ( ! in->is_gpu() ) {
        auto eltwise_pd = dnnl::eltwise_forward::primitive_desc(*ComputingContext::dnnl_gpu_engine,
        dnnl::prop_kind::forward_inference, op, src_md, dst_md, alpha, beta);
        auto eltwise_prim = dnnl::eltwise_forward(eltwise_pd);
        eltwise_prim.execute(*ComputingContext::dnnl_gpu_stream, eltwise_args);
        return;
    }
#endif
    auto eltwise_pd = dnnl::eltwise_forward::primitive_desc(*ComputingContext::dnnl_engine,
        dnnl::prop_kind::forward_inference, op, src_md, dst_md, alpha, beta);
    auto eltwise_prim = dnnl::eltwise_forward(eltwise_pd);
    eltwise_prim.execute(*ComputingContext::dnnl_stream, eltwise_args);
}

template<typename T>
void linear(T* src, T* weight, T* bias, T* dst, size_t batch, size_t outFeature, size_t inFeature ) {
    auto src_md = src->build_memory_desc( {1, batch, inFeature},  dnnl::memory::format_tag::abc);
    auto w_md = weight->build_memory_desc( {1, inFeature, outFeature}, dnnl::memory::format_tag::acb);
    dnnl::memory::desc b_md;
    if ( bias != nullptr) {
        b_md = bias->build_memory_desc( {1, 1, outFeature}, dnnl::memory::format_tag::abc);
    }
    auto dst_md = dst->build_memory_desc( {1, batch, outFeature}, dnnl::memory::format_tag::abc);

    std::unordered_map<int, dnnl::memory> matmul_args;
    matmul_args[DNNL_ARG_SRC] = src->build_memory(src_md);
    matmul_args[DNNL_ARG_WEIGHTS] = weight->build_memory(w_md);
    matmul_args[DNNL_ARG_DST] = dst->build_memory(dst_md);
    if ( bias != nullptr ) {
         matmul_args[DNNL_ARG_BIAS] = bias->build_memory( b_md );
    }
#ifdef _DNNL_GPU_
    if (  src->is_gpu() ) {
        dnnl::matmul::primitive_desc matmul_pd;
        if ( bias == nullptr) {
            matmul_pd = dnnl::matmul::primitive_desc(
                *ComputingContext::dnnl_gpu_engine, src_md, w_md, dst_md);
        } else {
            matmul_pd = dnnl::matmul::primitive_desc(
                *ComputingContext::dnnl_gpu_engine, src_md, w_md, b_md, dst_md);
        }
        auto matmul_prim = dnnl::matmul(matmul_pd);
        matmul_prim.execute(*ComputingContext::dnnl_gpu_stream, matmul_args);
        return;
    }
#endif

    dnnl::matmul::primitive_desc matmul_pd;
    if ( bias == nullptr) {
        matmul_pd = dnnl::matmul::primitive_desc(
            *ComputingContext::dnnl_engine, src_md, w_md, dst_md);
    } else {
        matmul_pd = dnnl::matmul::primitive_desc(
            *ComputingContext::dnnl_engine, src_md, w_md, b_md, dst_md);
    }
    auto matmul_prim = dnnl::matmul(matmul_pd);
    matmul_prim.execute(*ComputingContext::dnnl_stream, matmul_args);
}

template<typename T>
void simple_gemm(T* src, T* w, T* dst, dnnl::memory::desc src_md, dnnl::memory::desc w_md, dnnl::memory::desc dst_md) {
    dnnl::matmul::primitive_desc matmul_pd = dnnl::matmul::primitive_desc(*ComputingContext::dnnl_engine, src_md, w_md, dst_md);
    auto matmul_prim = dnnl::matmul(matmul_pd);

    std::unordered_map<int, dnnl::memory> matmul_args;
    matmul_args[DNNL_ARG_SRC] = dnnl::memory(src_md, *ComputingContext::dnnl_engine, src);
    matmul_args[DNNL_ARG_WEIGHTS] = dnnl::memory(w_md, *ComputingContext::dnnl_engine, w);
    matmul_args[DNNL_ARG_DST] = dnnl::memory(dst_md, *ComputingContext::dnnl_engine, dst);

    matmul_prim.execute(*ComputingContext::dnnl_stream, matmul_args);
}

#ifdef _DNNL_GPU_
void simple_gpu_gemm(cl_mem src, cl_mem w, cl_mem dst, dnnl::memory::desc src_md, dnnl::memory::desc w_md, dnnl::memory::desc dst_md) {
    dnnl::matmul::primitive_desc matmul_pd = dnnl::matmul::primitive_desc(*ComputingContext::dnnl_gpu_engine, src_md, w_md, dst_md);
    auto matmul_prim = dnnl::matmul(matmul_pd);

    std::unordered_map<int, dnnl::memory> matmul_args;
    matmul_args[DNNL_ARG_SRC] = dnnl::memory(src_md, *ComputingContext::dnnl_gpu_engine, src);
    matmul_args[DNNL_ARG_WEIGHTS] = dnnl::memory(w_md, *ComputingContext::dnnl_gpu_engine, w);
    matmul_args[DNNL_ARG_DST] = dnnl::memory(dst_md, *ComputingContext::dnnl_gpu_engine, dst);

    matmul_prim.execute(*ComputingContext::dnnl_gpu_stream, matmul_args);
}
#endif

template<typename T>
void layernrom(T* x, T* scale, T* bias, T* y, size_t batch_size, size_t hidden_dim, float eps) {
    auto src_md = x->build_memory_desc({batch_size, 1, hidden_dim}, dnnl::memory::format_tag::tnc);
    auto dst_md = y->build_memory_desc({batch_size, 1, hidden_dim}, dnnl::memory::format_tag::tnc);

    auto scale_md = x->build_memory_desc({hidden_dim}, dnnl::memory::format_tag::a);
    auto bias_md = y->build_memory_desc({hidden_dim}, dnnl::memory::format_tag::a);

    std::unordered_map<int, dnnl::memory> lnorm_args;
    lnorm_args[DNNL_ARG_SRC] = x->build_memory(src_md);
    lnorm_args[DNNL_ARG_DST] = y->build_memory(dst_md);
    lnorm_args[DNNL_ARG_SCALE] = x->build_memory(scale_md);
    lnorm_args[DNNL_ARG_SHIFT] = y->build_memory(bias_md);

#ifdef _DNNL_GPU_
    if ( x->is_gpu() ) {
        auto lnorm_pd = dnnl::layer_normalization_forward::primitive_desc(*ComputingContext::dnnl_gpu_engine,
        dnnl::prop_kind::forward_inference, src_md, dst_md, eps,
        dnnl::normalization_flags::use_scale | dnnl::normalization_flags::use_shift);
        auto lnorm_prim = dnnl::layer_normalization_forward(lnorm_pd);
        lnorm_prim.execute(*ComputingContext::dnnl_gpu_stream, lnorm_args);
        return;
    }
#endif

    auto lnorm_pd = dnnl::layer_normalization_forward::primitive_desc(*ComputingContext::dnnl_engine,
            dnnl::prop_kind::forward_inference, src_md, dst_md, eps,
            dnnl::normalization_flags::use_scale | dnnl::normalization_flags::use_shift);
    auto lnorm_prim = dnnl::layer_normalization_forward(lnorm_pd);
    lnorm_prim.execute(*ComputingContext::dnnl_stream, lnorm_args);
}

template <typename T, DataType DT>
void rmsnorm(T* x, T* scale,  T* y, size_t batch_size, size_t hidden_dim, float eps) {
    if ( DT != DataType::Float &&  DT != DataType::FP16) {
        vt_panic("DNNL rmsnor only support float and fp16!");
    }

#pragma omp parallel for
    for (size_t i = 0; i < batch_size; i++) {
        float rms = 0.0;
        if ( DT == DataType::Float) {
            for(size_t j = 0; j < hidden_dim; j++) {
                float v = x[i * hidden_dim + j];
                rms = rms + v * v;
            }
        }
        if ( DT == DataType::FP16) {
            for(size_t j = 0; j < hidden_dim; j++) {
                float v = fp16_to_fp32(x[i * hidden_dim + j]);
                rms = rms + v * v;
            }
        }

        rms = rms / (float)hidden_dim;
        rms = 1.0 / sqrt(rms + eps);

        if ( DT == DataType::Float) {
            for(size_t j = 0; j < hidden_dim; j++) {
                float v = x[i * hidden_dim + j];
                y[i * hidden_dim + j] = v * rms * scale[j];
            }
        }
        if ( DT == DataType::FP16) {
            for(size_t j = 0; j < hidden_dim; j++) {
                float v = fp16_to_fp32(x[i * hidden_dim + j]);
                y[i * hidden_dim + j] = fp32_to_fp16( v * rms * fp16_to_fp32(scale[j]) );
            }
        }
    }
}


template <typename T>
void rotary_embed(T* in, float* cos_sin, int* pos, T* out, size_t batch, size_t  heads, size_t tokens, size_t dims);

template <>
void rotary_embed<float>(float* in, float* cos_sin, int* pos, float* out, size_t batch, size_t  heads, size_t tokens, size_t dims) {
    for (size_t b = 0; b < batch; b++) {
        int p = pos[b];
        #pragma omp parallel for
        for (size_t t = 0; t < tokens; t++) {
            float* tab = cos_sin + (t + p) * dims * 2;
            for (size_t h = 0; h < heads; h++) {
                size_t offset = b * heads * tokens * dims + t * heads * dims + h * dims;
                for (size_t i = 0;  i < dims/2; i++) {
                    int ii = i + dims/2;
                    float x = in[i+offset];
                    float y = in[ii+offset];
                    out[i+offset] = (tab[i*2] * x - tab[i*2+1] * y);
                    out[ii+offset] = (tab[ii*2] * y + tab[ii*2+1] * x);
                }
            }
        }
    }
}

template <>
void rotary_embed<local_fp16_t>(local_fp16_t* in, float* cos_sin, int* pos, local_fp16_t* out, size_t batch, size_t  heads, size_t tokens, size_t dims) {
    for (size_t b = 0; b < batch; b++) {
        int p = pos[b];
        #pragma omp parallel for
        for (size_t h = 0; h < heads; h++) {
            for (size_t t = 0; t < tokens; t++) {
                size_t offset = b * heads * tokens * dims + h * tokens * dims + t * dims;
                float* tab = cos_sin + (t + p) * dims * 2;
                for (size_t i = 0;  i < dims/2; i++) {
                    int ii = i + dims/2;
                    float x = fp16_to_fp32(in[i+offset]);
                    float y = fp16_to_fp32(in[ii+offset]);
                    out[i+offset] = fp32_to_fp16(tab[i*2] * x - tab[i*2+1] * y);
                    out[ii+offset] = fp32_to_fp16(tab[ii*2] * y + tab[ii*2+1] * x);
                }
            }
        }
    }
}

template <typename T>
void transpose_0213(T* in, T* out, size_t batch, size_t heads, size_t tokens, size_t dims) {
    size_t items = batch * heads * tokens * dims;

    #pragma omp parallel for
    for ( size_t i = 0; i < items; i++) {
        size_t d = i % dims;
        size_t t = (i / dims) % tokens;
        size_t h = (i / (dims*tokens)) % heads;
        size_t b = i / (dims*tokens*heads);
        size_t target = b * (dims*tokens*heads) + t * heads * dims + h * dims + d;
        out[i] = in[target];
    }
}

template<typename T>
void query_key(T* query, T* key, T* qk, size_t batch, size_t newTokens, size_t fullTokens, size_t hidden ) {
    auto q_md = query->build_memory_desc( {batch, newTokens, hidden},  dnnl::memory::format_tag::abc);
    auto k_md = key->build_memory_desc( {batch, hidden, fullTokens}, dnnl::memory::format_tag::acb);
    auto qk_md = qk->build_memory_desc( {batch, newTokens, fullTokens}, dnnl::memory::format_tag::abc);

    std::unordered_map<int, dnnl::memory> matmul_args;
    matmul_args[DNNL_ARG_SRC] = query->build_memory(q_md);
    matmul_args[DNNL_ARG_WEIGHTS] = key->build_memory(k_md);
    matmul_args[DNNL_ARG_DST] = qk->build_memory(qk_md);

    dnnl::post_ops matmul_ops;
    matmul_ops.append_eltwise(dnnl::algorithm::eltwise_linear, 1.0/sqrt(hidden), 0.0);
    dnnl::primitive_attr matmul_attr;
    matmul_attr.set_post_ops(matmul_ops);

#ifdef _DNNL_GPU_
    if ( query->is_gpu() ) {
        dnnl::matmul::primitive_desc matmul_pd;
        matmul_pd = dnnl::matmul::primitive_desc(*ComputingContext::dnnl_gpu_engine, q_md, k_md, qk_md, matmul_attr);

        auto matmul_prim = dnnl::matmul(matmul_pd);
        matmul_prim.execute(*ComputingContext::dnnl_gpu_stream, matmul_args);
        return;
    }
#endif

    dnnl::matmul::primitive_desc matmul_pd;
    matmul_pd = dnnl::matmul::primitive_desc(*ComputingContext::dnnl_engine, q_md, k_md, qk_md, matmul_attr);

    auto matmul_prim = dnnl::matmul(matmul_pd);
    matmul_prim.execute(*ComputingContext::dnnl_stream, matmul_args);
}

template<typename T>
void softmax(T* src, T* dst, size_t batch, size_t hidden ) {
    auto src_md = src->build_memory_desc( {batch, hidden},  dnnl::memory::format_tag::nc);
    auto dst_md = dst->build_memory_desc( {batch, hidden}, dnnl::memory::format_tag::nc);

    const int axis = 1;
    std::unordered_map<int, dnnl::memory> softmax_args;
    softmax_args[DNNL_ARG_SRC] = src->build_memory(src_md);
    softmax_args[DNNL_ARG_DST] = src->build_memory(dst_md);
    
#ifdef _DNNL_GPU_ 
    if ( src->is_gpu() ) {
        auto softmax_pd = dnnl::softmax_forward::primitive_desc(*ComputingContext::dnnl_gpu_engine,
            dnnl::prop_kind::forward_inference, dnnl::algorithm::softmax_accurate, src_md,
            dst_md, axis);
        auto softmax_prim = dnnl::softmax_forward(softmax_pd);
        softmax_prim.execute(*ComputingContext::dnnl_gpu_stream, softmax_args);
        return;
    }
#endif

    auto softmax_pd = dnnl::softmax_forward::primitive_desc(*ComputingContext::dnnl_engine,
            dnnl::prop_kind::forward_inference, dnnl::algorithm::softmax_accurate, src_md,
            dst_md, axis);
    auto softmax_prim = dnnl::softmax_forward(softmax_pd);
    softmax_prim.execute(*ComputingContext::dnnl_stream, softmax_args);
}


template<typename T>
void attn(T* xll, T* value, T* out, size_t batch, size_t newTokens, size_t fullTokens, size_t hidden ) {
    auto xll_md = xll->build_memory_desc( {batch, newTokens, fullTokens},  dnnl::memory::format_tag::abc);
    auto v_md = value->build_memory_desc( {batch, fullTokens, hidden}, dnnl::memory::format_tag::abc);
    auto o_md = out->build_memory_desc( {batch, newTokens, hidden}, dnnl::memory::format_tag::abc);

    dnnl::matmul::primitive_desc matmul_pd;

    matmul_pd = dnnl::matmul::primitive_desc(*ComputingContext::dnnl_engine, xll_md, v_md, o_md);

    auto matmul_prim = dnnl::matmul(matmul_pd);

    std::unordered_map<int, dnnl::memory> matmul_args;
    matmul_args[DNNL_ARG_SRC] = xll->build_memory(xll_md);
    matmul_args[DNNL_ARG_WEIGHTS] = value->build_memory(v_md);
    matmul_args[DNNL_ARG_DST] = out->build_memory(o_md);

    matmul_prim.execute(*ComputingContext::dnnl_stream, matmul_args);
}

template <typename T>
void gelu(T* in, T* out, size_t items);

template <>
void gelu<float>(float* src, float* target, size_t items) {
    #pragma omp parallel for
    for ( size_t i = 0; i < items; i++) {
        float value = src[i];
        target[i] = value * (0.5F + 0.5F * tanhf(value * (0.79788456F + 0.03567741F * value * value)));
        //target[i] = value * normcdf(value);
    }
}

template <>
void gelu<local_fp16_t>(local_fp16_t* src, local_fp16_t* target, size_t items) {
    #pragma omp parallel for
    for ( size_t i = 0; i < items; i++) {
        float value = fp16_to_fp32(src[i]);
        target[i] = fp32_to_fp16(value * (0.5F + 0.5F * tanhf(value * (0.79788456F + 0.03567741F * value * value))));
    }
}

template <typename T>
void silu_product(T* a, T* b,  T* out, size_t items);

template <>
void silu_product<float>(float* in_act, float* in,  float* out, size_t items) {
    #pragma omp parallel for
    for ( size_t i = 0; i < items; i++) {
        float act = in_act[i];
        float in_ = in[i];
        out[i] = act / (1.f + expf(-act)) * in_;
    }
}

template <>
void silu_product<local_fp16_t>(local_fp16_t* in_act, local_fp16_t* in,  local_fp16_t* out, size_t items) {
    #pragma omp parallel for
    for ( size_t i = 0; i < items; i++) {
        float act = fp16_to_fp32( in_act[i] );
        float in_ = fp16_to_fp32( in[i] );
        out[i] = fp32_to_fp16( act / (1.f + expf(-act)) * in_ );
    }
}

template <typename T>
void easy_top1(T* logits, int* out, size_t batch, size_t vocab_size);

template <>
void easy_top1<float>(float* logits, int* out, size_t batch, size_t vocab_size) {
    #pragma omp parallel for
    for (size_t b = 0; b < batch; b++) {
        float* src = logits + b * vocab_size;

        float max_v = std::numeric_limits<float>::min();
        int max_i = -1;
        for (int i = 0; i < (int)vocab_size; i++) {
            if ( src[i] > max_v ) {
                max_v = src[i];
                max_i = i;
            }
        }
        out[b] = max_i;
    }
}

template <>
void easy_top1<local_fp16_t>(local_fp16_t* logits, int* out, size_t batch, size_t vocab_size) {
    #pragma omp parallel for
    for (size_t b = 0; b < batch; b++) {
        local_fp16_t* src = logits + b * vocab_size;

        float max_v = std::numeric_limits<float>::min();
        int max_i = -1;
        for (int i = 0; i < (int)vocab_size; i++) {
            float v = fp16_to_fp32( src[i] );
            if ( v > max_v ) {
                max_v = v;
                max_i = i;
            }
        }
        out[b] = max_i;
    }
}



struct TopItem {
    float v;
    int i;
    TopItem(int i_, float v_) : v(v_), i(i_) {};
};

class Compare {
public:
    bool operator() (TopItem foo, TopItem bar) {
        return foo.v > bar.v;
    }
};


int do_sampling(std::priority_queue<TopItem, std::vector<TopItem>, Compare>& topk_, float temp, float randx) {
    std::vector<TopItem> topk;
    while ( topk_.size() > 0 ) {
        topk.push_back( topk_.top() );
        topk_.pop();
    }
    const int K = topk.size();

    float sum = 1.0;
    for(auto i = 0; i < K - 1; i++) {
        topk[i].v = expf( (topk[i].v - topk[K-1].v) / temp );
        sum = sum + topk[i].v;
    }
    topk[K-1].v = 0;

    float thres = 0.0;
    for(auto i = 0; i < K; i++) {
        thres += topk[i].v / sum;
        if ( thres >= randx ) {
            return topk[i].i;
        }
    }
    return topk[K-1].i;
}

template <typename T>
void easy_top3(T* logits, int* out, size_t batch, size_t vocab_size, float temp, float randx);

template <>
void easy_top3<float>(float* logits, int* out, size_t batch, size_t vocab_size, float temp, float randx) {

    #pragma omp parallel for
    for (size_t b = 0; b < batch; b++) {
        float* src = logits + b * vocab_size;

        std::priority_queue<TopItem, std::vector<TopItem>, Compare> topk;
        for (int i = 0; i < 3; i++) {
            topk.push( {i, src[i]} );
        }

        for (int i = 3; i < (int)vocab_size; i++) {
            float v = src[i];
            if ( v >  topk.top().v ) {
                topk.pop();
                topk.push({i, v});
            }
        }

        out[b] = do_sampling(topk, temp, randx);
    }

}

template <>
void easy_top3<local_fp16_t>(local_fp16_t* logits, int* out, size_t batch, size_t vocab_size, float temp, float randx) {

    #pragma omp parallel for
    for (size_t b = 0; b < batch; b++) {
        local_fp16_t* src = logits + b * vocab_size;

        std::priority_queue<TopItem, std::vector<TopItem>, Compare> topk;
        for (int i = 0; i < 3; i++) {
            topk.push( {i, fp16_to_fp32(src[i])} );
        }

        for (int i = 3; i < (int)vocab_size; i++) {
            float v = fp16_to_fp32(src[i]);
            if ( v >  topk.top().v ) {
                topk.pop();
                topk.push({i, v});
            }
        }

        out[b] = do_sampling(topk, temp, randx);
    }
}


}}
#endif
