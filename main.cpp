#include <iostream>
#include <cstdint>
#include <atomic>
#include <gnuradio/block.h>
#include <gnuradio/sync_block.h>
#include <gnuradio/top_block.h>
#include <gnuradio/io_signature.h>

#if GNURADIO_VER == 38
#define GRSPTR boost::shared_ptr
#elif GNURADIO_VER == 37
#define GRSPTR boost::shared_ptr
#else
#define GRSPTR std::shared_ptr
#endif


class cnt_source : virtual public gr::sync_block
{
public:
    typedef GRSPTR<cnt_source> sptr;
    static sptr make(size_t nitems)
    {
        return gnuradio::get_initial_sptr(new cnt_source(nitems));
    }
    cnt_source(size_t nitems)
    : sync_block("cnt_source",
                 gr::io_signature::make(0, 0, 0),
                 gr::io_signature::make(1, 1, sizeof(uint32_t))),
                 d_nitems(nitems)
    {
    }
    virtual ~cnt_source() {};
    int work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& output_items)
    {
        uint32_t* o = (uint32_t*)output_items[0];
        if(d_cnt >= d_nitems)
            return -1;
        for (size_t n = 0; n < noutput_items; n++)
        {
            if(d_cnt == d_nitems)
                return n;
            o[n] = d_cnt;
            if(d_tag_interval && (d_cnt % d_tag_interval == 0))
            {
                uint64_t nn = nitems_written(0) + n;
//                std::cerr<<">add_item_tag("<<nn<<","<<d_cnt<<"\n";
                add_item_tag(0, nn, pmt::intern("cnt_tag"), pmt::from_long(d_cnt));
//                std::cerr<<"<add_item_tag\n";
                d_tags++;
            }
            d_cnt++;
        }
        return noutput_items;
    }
    void reset_counters()
    {
        d_cnt = 0;
        d_tags = 0;
    }
    uint32_t get_counter()
    {
        return d_cnt;
    }
    void set_tag_interval(uint32_t in)
    {
        d_tag_interval = in;
    }
    uint32_t get_tags()
    {
        return d_tags;
    }
private:
    uint32_t d_tag_interval{0};
    std::atomic<uint32_t> d_tags{0};
    std::atomic<uint32_t> d_cnt{0};
    uint32_t d_nitems;
};

class cnt_sink : virtual public gr::sync_block
{
public:
    typedef GRSPTR<cnt_sink> sptr;
    static sptr make()
    {
        return gnuradio::get_initial_sptr(new cnt_sink());
    }
    cnt_sink()
    : sync_block("cnt_sink",
                 gr::io_signature::make(1, 1, sizeof(uint32_t)),
                 gr::io_signature::make(0, 0, 0))
    {
    }
    virtual ~cnt_sink() {};
    int work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& output_items)
    {
        uint32_t* i = (uint32_t*)input_items[0];
        std::vector<gr::tag_t> work_tags;
        get_tags_in_window(work_tags, 0, 0, noutput_items);
        for (const auto& tag : work_tags)
        {
            d_tags++;
//            std::cerr<<"found tag "<<tag.offset<<"\n";
            if(i[tag.offset - nitems_read(0) - history()] != pmt::to_long(tag.value))
            {
                d_offset_tags++;
                d_last_tag_offset = i[tag.offset - nitems_read(0) - history()] - pmt::to_long(tag.value);
            }
        }
        for (size_t n = 0; n < noutput_items; n++)
        {
            d_cnt ++;
        }
        return noutput_items;
    }
    void reset_counters()
    {
        d_cnt = 0;
        d_tags = 0;
        d_offset_tags = 0;
    }
    uint32_t get_counter()
    {
        return d_cnt;
    }
    uint32_t get_tags()
    {
        return d_tags;
    }
    uint32_t get_offset_tags()
    {
        return d_offset_tags;
    }
    int32_t get_last_offset()
    {
        return d_last_tag_offset;
    }
private:
    std::atomic<uint32_t> d_cnt{0};
    std::atomic<uint32_t> d_tags{0};
    std::atomic<uint32_t> d_offset_tags{0};
    int32_t d_last_tag_offset{0};
};

class hist_block : virtual public gr::sync_block
{
public:
    typedef GRSPTR<hist_block> sptr;
    static sptr make()
    {
        return gnuradio::get_initial_sptr(new hist_block());
    }
    hist_block()
    : sync_block("hist_block",
                 gr::io_signature::make(1, 1, sizeof(uint32_t)),
                 gr::io_signature::make(1, 1, sizeof(uint32_t)))
    {
    }
    virtual ~hist_block() {};
    int work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& output_items)
    {
        uint32_t* o = (uint32_t*)output_items[0];
        uint32_t* i = (uint32_t*)input_items[0];
//        std::cerr<<"hist_block::work "<<nitems_read(0)<<","<<nitems_written(0)<<"\n";
        for (size_t n = 0; n < noutput_items; n++)
        {
            o[n] = i[n + history() - d_delay];
        }
        return noutput_items;
    }
    unsigned get_delay()
    {
        return d_delay;
    }
    void set_delay(unsigned delay)
    {
        d_delay = delay;
        if(d_delay >= history())
            d_delay = history() - 1;
    }
private:
    unsigned d_delay{0};
};

constexpr int CONNECT_SINK        = 0x00000001;
constexpr int DISCONNECT_SINK     = 0x00000002;
constexpr int CHANGE_PROC         = 0x00000004;
constexpr int EXPAND_ALIGN_SRC    = 0x00000008;
constexpr int SHRINK_ALIGN_SRC    = 0x00000010;
constexpr int EXPAND_HISTORY      = 0x00000020;
constexpr int SHRINK_HISTORY      = 0x00000040;
constexpr int ADD_LARGE_HISTORY   = 0x00000080;
constexpr int REMOVE_LARGE_HISTORY= 0x00000100;
constexpr int ADD_LARGE_ALIGN     = 0x00000200;
constexpr int REMOVE_LARGE_ALIGN  = 0x00000400;
constexpr int CHANGE_LARGE_HIST   = 0x00000800;
constexpr int CHANGE_SMALL_HIST   = 0x00001000;
constexpr int NO_HISTORY          = 0x00002000;
constexpr int DELAY               = 0x00004000;
constexpr int DELAY_DECL          = 0x00008000;

constexpr unsigned src_item_count = 1000000;
constexpr unsigned test_item_diff =   50000;

void do_wait(cnt_source::sptr src, unsigned to)
{
    while(src->get_counter() < to)
        if(src->get_counter() >= src_item_count - 1)
        {
            std::cerr<<"Failed: sample limit reached\n";
            exit(2);
        }
}

void do_the_test(int t, std::string descr)
{
    gr::top_block_sptr tb = gr::make_top_block("top");
    cnt_source::sptr src = cnt_source::make(src_item_count);
    cnt_sink::sptr dst0 = cnt_sink::make();
    cnt_sink::sptr dst1 = cnt_sink::make();
    cnt_sink::sptr dst2 = cnt_sink::make();
    hist_block::sptr cpy = hist_block::make();
    src->set_output_multiple(100);
    src->set_tag_interval(101);
    if(!t&NO_HISTORY)
        cpy->set_history(5000);
    if(t&DELAY)
        cpy->set_delay(2000);
    if(t&DELAY_DECL)
        cpy->declare_sample_delay(2000);

    std::cout<<"\n                "<<descr<<"\n";
    tb->connect(src, 0, cpy, 0);
    tb->connect(cpy, 0, dst0, 0);
    if(t&REMOVE_LARGE_HISTORY)
    {
        dst2->set_history(7000);
        tb->connect(src, 0, dst2, 0);
    }
    if(t&REMOVE_LARGE_ALIGN)
    {
        dst2->set_output_multiple(200);
        tb->connect(src, 0, dst2, 0);
    }
    tb->start(4096);
    if(t)
    {
        do_wait(src, src->get_counter() + test_item_diff);
        tb->lock();
    }
    if(t&EXPAND_HISTORY)
    {
        cpy->set_history(5500);
    }
    if(t&SHRINK_HISTORY)
    {
        cpy->set_history(3500);
    }
    if(t&CHANGE_PROC)
    {
        tb->disconnect(src, 0, cpy, 0);
        tb->disconnect(cpy, 0, dst0, 0);
        cpy=hist_block::make();
        if(t&CHANGE_LARGE_HIST)
            cpy->set_history(6000);
        else if(t&CHANGE_SMALL_HIST)
            cpy->set_history(3000);
        else
            cpy->set_history(5000);
        tb->connect(src, 0, cpy, 0);
        tb->connect(cpy, 0, dst0, 0);
    }
    if(t&EXPAND_ALIGN_SRC)
    {
        src->set_output_multiple(200);
    }
    if(t&SHRINK_ALIGN_SRC)
    {
        src->set_output_multiple(64);
    }
    if(t & CONNECT_SINK)
    {
        tb->connect(cpy, 0, dst1, 0);
        std::cout<<"Src counter on connect: "<<src->get_counter()<<"\n";
        if(t & DISCONNECT_SINK)
        {
            tb->unlock();
            do_wait(src, src->get_counter() + test_item_diff);
            tb->lock();
            tb->disconnect(cpy, 0, dst1, 0);
            std::cerr<<"Src counter on disconnect: "<<src->get_counter()<<"\n";
        }
    }
    if(t&(REMOVE_LARGE_HISTORY|REMOVE_LARGE_ALIGN))
    {
        tb->disconnect(src, 0, dst2, 0);
    }
    if(t&ADD_LARGE_HISTORY)
    {
        dst2->set_history(7000);
        tb->connect(src, 0, dst2, 0);
    }
    if(t&ADD_LARGE_ALIGN)
    {
        dst2->set_output_multiple(500);
        tb->connect(src, 0, dst2, 0);
    }
    if(t)
        tb->unlock();
    tb->wait();
    std::cout
        <<" Sent: "<<src->get_counter()<<" items "<<src->get_tags()<<" tags\n"
        <<"Sink0\n"
        <<" Received: "<<dst0->get_counter()<<" items "<<dst0->get_tags()<<" tags\n"
        <<" Lost: "<<(src->get_counter()-dst0->get_counter())<<" items "<<(src->get_tags()-dst0->get_tags())<<" tags\n"
        <<" Offset tags: "<<dst0->get_offset_tags()<<" last offset="<<dst0->get_last_offset()
        <<"\n"
        <<"Sink1\n"
        <<" Received: "<<dst1->get_counter()<<" items "<<dst1->get_tags()<<" tags\n"
        <<" Offset tags on sink1: "<<dst1->get_offset_tags()<<" last offset="<<dst1->get_last_offset()
        <<"\n";
}







int main()
{
    do_the_test(0,"No lock\n");
    do_the_test(CONNECT_SINK,"Connect sink\n");
    do_the_test(CONNECT_SINK|DISCONNECT_SINK,"Connect/disconnect sink\n");
    do_the_test(EXPAND_HISTORY,"Expand history\n");
    do_the_test(SHRINK_HISTORY,"Shrink history\n");
    do_the_test(CHANGE_PROC,"Change processing block\n");
    do_the_test(CHANGE_PROC|CHANGE_LARGE_HIST,"Change processing block to a block with larger history\n");
    do_the_test(CHANGE_PROC|CHANGE_SMALL_HIST,"Change processing block to a block with smaller history\n");
    do_the_test(EXPAND_ALIGN_SRC,"Expand src alignment\n");
    do_the_test(SHRINK_ALIGN_SRC,"Shrink src alignment\n");
    do_the_test(ADD_LARGE_HISTORY,"Add a sink with a large history\n");
    do_the_test(REMOVE_LARGE_HISTORY,"Remove a sink with a large history\n");
    do_the_test(ADD_LARGE_ALIGN,"Add a sink with a large alignment\n");
    do_the_test(REMOVE_LARGE_ALIGN,"Remove a sink with a large alignment\n");
    do_the_test(CHANGE_PROC|NO_HISTORY,"Change processing block without a history\n");
    do_the_test(CONNECT_SINK|DELAY,"Connect sink, processing block with undeclared delay\n");
    do_the_test(CONNECT_SINK|DELAY|DELAY_DECL,"Connect sink, processing block with declared delay\n");
    return 0;
}
