#include <osmium/handler.hpp>

using namespace boost::python;

typedef osmium::index::map::SparseTable<osmium::unsigned_object_id_type, osmium::Location> sparse_index_type;

class BaseHandler : public osmium::handler::Handler {

protected:
    enum pre_handler {
        no_handler,
        location_handler,
        area_handler
    };


public:
    enum location_index {
        sparse_index
    };


// handler functions
virtual void node(const osmium::Node&) const = 0;
virtual void way(const osmium::Way&) const = 0;
virtual void relation(const osmium::Relation&) const = 0;
virtual void changeset(const osmium::Changeset&) const = 0;
virtual void area(const osmium::Area&) const = 0;


private:
template <typename IDX>
void apply_with_location(osmium::io::Reader &r) {
    IDX index;
    osmium::handler::NodeLocationsForWays<IDX> location_handler(index);
    location_handler.ignore_errors();
    osmium::apply(r, location_handler, *this);
}

template <typename IDX>
void apply_with_area(osmium::io::Reader &r,
                     osmium::area::MultipolygonCollector<osmium::area::Assembler> &collector) {

    IDX index;
    osmium::handler::NodeLocationsForWays<IDX> location_handler(index);
    location_handler.ignore_errors();

    osmium::apply(r, location_handler, *this,
                  collector.handler([this](const osmium::memory::Buffer& area_buffer) {
                       osmium::apply(area_buffer, *this);
                  })
                 );
}


protected:
void apply(const std::string &filename, osmium::osm_entity_bits::type types,
           pre_handler pre = no_handler,
           location_index idx = sparse_index) {

    switch (pre) {
    case no_handler:
        {
            osmium::io::Reader reader(filename, types);
            osmium::apply(reader, *this);
            reader.close();
            break;
        }
    case location_handler:
        {
            osmium::io::Reader reader(filename, types);
            switch (idx) {
                case sparse_index:
                    apply_with_location<sparse_index_type>(reader);
                    break;
            }
            reader.close();
            break;
        }
    case area_handler:
        {
            osmium::area::Assembler::config_type assembler_config;
            osmium::area::MultipolygonCollector<osmium::area::Assembler> collector(assembler_config);

            osmium::io::Reader reader1(filename);
            collector.read_relations(reader1);
            reader1.close();

            osmium::io::Reader reader2(filename);

            switch (idx) {
                case sparse_index:
                    apply_with_area<sparse_index_type>(reader2, collector);
                    break;
            }
            reader2.close();
            break;
        }
    }
}



};


struct SimpleHandlerWrap: BaseHandler, wrapper<BaseHandler> {

    void node(const osmium::Node& node) const {
        if (override f = this->get_override("node"))
            f(boost::ref(node));
    }

    void default_node(const osmium::Node&) const {
    }

    void way(const osmium::Way& way) const {
        if (override f = this->get_override("way"))
            f(boost::ref(way));
    }

    void default_way(const osmium::Way&) const {
    }

    void relation(const osmium::Relation& rel) const {
        if (override f = this->get_override("relation"))
            f(boost::ref(rel));
    }

    void default_relation(const osmium::Relation&) const {
    }

    void changeset(const osmium::Changeset& cs) const {
        if (override f = this->get_override("changeset"))
            f(boost::ref(cs));
    }

    void default_changeset(const osmium::Changeset&) const {
    }

    void area(const osmium::Area& area) const {
        if (override f = this->get_override("area"))
            f(boost::ref(area));
    }

    void default_area(const osmium::Area&) const {
    }

    void apply_file(const std::string &filename, bool locations = false,
                    location_index idx = sparse_index)
    {
        osmium::osm_entity_bits::type entities = osmium::osm_entity_bits::nothing;
        BaseHandler::pre_handler handler = locations?
                                            BaseHandler::location_handler
                                            :BaseHandler::no_handler;

        if (this->get_override("area"))
        {
            entities = osmium::osm_entity_bits::object;
            handler = BaseHandler::area_handler;
        } else {
            if (locations || this->get_override("node"))
                entities |= osmium::osm_entity_bits::node;
            if (this->get_override("way"))
                entities |= osmium::osm_entity_bits::way;
            if (this->get_override("relation"))
                entities |= osmium::osm_entity_bits::relation;
        }

        if (this->get_override("changeset"))
            entities |= osmium::osm_entity_bits::changeset;

        apply(filename, entities, handler, idx);
    }
};
