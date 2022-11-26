/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#define OGT_VOX_IMPLEMENTATION

#include "VOXLoader.h"

namespace rvx
{

// a helper function to load a magica voxel scene given a filename.
const ogt_vox_scene* load_vox_scene(const char* filename, uint32_t scene_read_flags = 0)
{
    // open the file
#if defined(_MSC_VER) && _MSC_VER >= 1400
    FILE* fp;
    if(0 != fopen_s(&fp, filename, "rb"))
        fp = 0;
#else
    FILE* fp = fopen(filename, "rb");
#endif
    if(!fp)
        return NULL;

    // get the buffer size which matches the size of the file
    fseek(fp, 0, SEEK_END);
    uint32_t buffer_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // load the file into a memory buffer
    uint8_t* buffer = new uint8_t[buffer_size];
    fread(buffer, buffer_size, 1, fp);
    fclose(fp);

    // construct the scene from the buffer
    const ogt_vox_scene* scene = ogt_vox_read_scene_with_flags(buffer, buffer_size, scene_read_flags);

    // the buffer can be safely deleted once the scene is instantiated.
    delete[] buffer;

    return scene;
}

// a helper function to save a magica voxel scene to disk.
void save_vox_scene(const char* pcFilename, const ogt_vox_scene* scene)
{
    // save the scene back out.
    uint32_t buffersize = 0;
    uint8_t* buffer     = ogt_vox_write_scene(scene, &buffersize);
    if(!buffer)
        return;

        // open the file for write
#if defined(_MSC_VER) && _MSC_VER >= 1400
    FILE* fp;
    if(0 != fopen_s(&fp, pcFilename, "wb"))
        fp = 0;
#else
    FILE* fp = fopen(pcFilename, "wb");
#endif
    if(!fp)
    {
        ogt_vox_free(buffer);
        return;
    }

    fwrite(buffer, buffersize, 1, fp);
    fclose(fp);
    ogt_vox_free(buffer);
}

const ogt_vox_scene* VOXLoader::LoadVOX(const char* fileName, bool retry)
{
    const ogt_vox_scene* scene = nullptr;
    do
    {
        try
        {
            scene = load_vox_scene(fileName);
        }
        catch(std::exception&)
        {
            scene = nullptr;
        }
    } while(scene == nullptr && retry);
    return scene;
}

bool VOXLoader::AllVoxelsSameColor(
    uint8_t* data, int sx, int ex, int y, int z, const ogt_vox_model* model, Matrix* inverse, uint8_t c, const int sizes[3])
{
    while(sx <= ex)
        if(data[GetOffset(sx++, y, z, model, inverse, sizes)] != c)
            return false;
    return true;
}

void VOXLoader::RemoveVoxels(uint8_t* data, int sx, int ex, int y, int z, const ogt_vox_model* model, Matrix* inverse, const int sizes[3])
{
    while(sx <= ex)
        data[GetOffset(sx++, y, z, model, inverse, sizes)] = 0;
}

int VOXLoader::GetOffset(int x, int y, int z, const ogt_vox_model* model, Matrix* inverse, const int sizes[3])
{
    if(inverse != nullptr)
    {
        // translate world space (x, y, z) into model space, accounting for mirrors
        if(sizes[0] < 0)
            x = -sizes[0] - 1 - x;
        if(sizes[1] < 0)
            y = -sizes[0] - 1 - y;
        if(sizes[2] < 0)
            z = -sizes[2] - 1 - z;

        Transform(x, y, z, inverse);

        x = abs(x);
        y = abs(y);
        z = abs(z);
    }

    const int sx = model->size_x;
    const int sy = model->size_y;

    return x + abs(y * sx) + abs(z * sx * sy);
}

void VOXLoader::Transform(int& x, int& y, int& z, Matrix* transform)
{
    auto tv = Vector3Transform(Vector3 {(float)x, (float)y, (float)z}, *transform);
    x       = (int)tv.x;
    y       = (int)tv.y;
    z       = (int)tv.z;
}

void VOXLoader::ImportVOX(const ogt_vox_scene* vox, Scene& scene, bool optimize)
{
    scene.m_palette.clear();
    scene.m_palette.resize(256);
    for(int i = 0; i < 256; i++)
    {
        const auto& c        = vox->palette.color[i];
        scene.m_palette[i].r = c.r;
        scene.m_palette[i].g = c.g;
        scene.m_palette[i].b = c.b;
        scene.m_palette[i].a = c.a;
    }

    int an = 100; // automatic number, if not present
    int en = 0;
    scene.m_areas.clear();
    scene.m_edges.clear();
    for(uint32_t ii = 0; ii < vox->num_instances; ii++)
    {
        auto&       area          = scene.m_areas.emplace_back();
        const auto& instance      = vox->instances[ii];
        const auto& model         = vox->models[instance.model_index];
        const auto& ogt_transform = vox->instances[ii].transform;

        area.m_no = an++;
        if(instance.name != NULL)
        {
            area.m_name = instance.name;
            if(instance.name[2] == 0 && isdigit(instance.name[0]) && isdigit(instance.name[1]))
            {
                // override no with instance name if it's two digits
                area.m_no = (instance.name[0] - '0') * 10 + (instance.name[1] - '0');
            }
        }

        Matrix transform;
        transform.m0  = ogt_transform.m00;
        transform.m1  = ogt_transform.m01;
        transform.m2  = ogt_transform.m02;
        transform.m3  = ogt_transform.m03;
        transform.m4  = ogt_transform.m10;
        transform.m5  = ogt_transform.m11;
        transform.m6  = ogt_transform.m12;
        transform.m7  = ogt_transform.m13;
        transform.m8  = ogt_transform.m20;
        transform.m9  = ogt_transform.m21;
        transform.m10 = ogt_transform.m22;
        transform.m11 = ogt_transform.m23;

        // ignore translation
        transform.m12 = 0;
        transform.m13 = 0;
        transform.m14 = 0;
        transform.m15 = 1;

        Matrix inverse    = MatrixInvert(transform);
        bool   isIdentity = (inverse.m0 == 1 && inverse.m5 == 1 && inverse.m10 == 1 && inverse.m15 == 1);
        if(isIdentity)
            isIdentity &= (inverse.m1 == 0 && inverse.m2 == 0 && inverse.m3 == 0 && inverse.m4 == 0);
        if(isIdentity)
            isIdentity &= (inverse.m6 == 0 && inverse.m7 == 0 && inverse.m8 == 0 && inverse.m9 == 0);
        if(isIdentity)
            isIdentity &= (inverse.m11 == 0 && inverse.m12 == 0 && inverse.m13 == 0 && inverse.m14 == 0);

        // skip matrix ops when there are no transforms
        Matrix* inversep = isIdentity ? nullptr : &inverse;

        // transform sizes
        int space_size_x = model->size_x;
        int space_size_y = model->size_y;
        int space_size_z = model->size_z;
        Transform(space_size_x, space_size_y, space_size_z, &transform);
        const int sizes[3] = {space_size_x, space_size_y, space_size_z};
        space_size_x       = abs(space_size_x);
        space_size_y       = abs(space_size_y);
        space_size_z       = abs(space_size_z);

        // translation
        const int dx = -(int)space_size_x / 2 + (int)ogt_transform.m30;
        const int dy = -(int)space_size_y / 2 + (int)ogt_transform.m31;
        const int dz = -(int)space_size_z / 2 + (int)ogt_transform.m32;

        // copy in case the are instanced and we modify them
        uint8_t* voxel_copy = (uint8_t*)malloc(space_size_x * space_size_y * space_size_z);
        memcpy(voxel_copy, model->voxel_data, space_size_x * space_size_y * space_size_z);

        // space coords of 0,0,0
        area.m_sx = dx;
        area.m_sy = dy;
        area.m_sz = dz;

        for(int y = 0; y < space_size_y; y++)
        {
            for(int z = 0; z < space_size_z; z++)
            {
                for(int x = 0; x < space_size_x; x++)
                {
                    const int  off = GetOffset(x, y, z, model, inversep, sizes);
                    const auto c   = voxel_copy[off];
                    if(c == 0)
                        continue;

                    // optimisation - draw a line or even a rectangle in one go if possible
                    int sx = x;
                    int sz = z;
                    int ez = z;

#ifdef RVX_EDGES
                    // edge markers
                    if(c == RVX_EDGE_L || c == RVX_EDGE_R)
                    {
                        Edge e;

                        // find x/y/z dimensions of the edge
                        while(x < space_size_x - 1 && voxel_copy[GetOffset(x + 1, y, z, model, inversep, sizes)] == c)
                        {
                            x++;
                        }

                        // find ez
                        while(ez < space_size_z - 1 && AllVoxelsSameColor(voxel_copy, sx, x, y, ez + 1, model, inversep, c, sizes))
                        {
                            // fill in with zeros so we don't redraw later
                            //RemoveVoxels(voxel_copy, sx, x, y, ez + 1, model, inversep, sizes);
                            ez++;
                        }

                        // find spacing
                        int spacing = 1;
                        while(voxel_copy[GetOffset(x, y + 1 + spacing, ez, model, inversep, sizes)] == 0)
                        {
                            spacing++;
                        }

                        // take colors from next (y+1) plane
                        e.top_left_col     = voxel_copy[GetOffset(sx, y + 1, ez, model, inversep, sizes)];
                        e.top_right_col    = voxel_copy[GetOffset(x, y + 1, ez, model, inversep, sizes)];
                        e.bottom_left_col  = voxel_copy[GetOffset(sx, y + 1, sz, model, inversep, sizes)];
                        e.bottom_right_col = voxel_copy[GetOffset(x, y + 1, sz, model, inversep, sizes)];

                        // find edge_width
                        int width = 0;
                        if(c == RVX_EDGE_L)
                        {
                            while(voxel_copy[GetOffset(sx + width, y + 1, ez, model, inversep, sizes)] == e.top_left_col)
                            {
                                width++;
                            }
                        }
                        else if(c == RVX_EDGE_R)
                        {
                            while(voxel_copy[GetOffset(x - width, y + 1, ez, model, inversep, sizes)] == e.top_right_col)
                            {
                                width++;
                            }
                        }
                        if(width >= x - sx)
                        {
                            // same color, doesn't matter
                            width = (x - sx) / 2;
                        }

                        // find edge_height
                        int height = 0;
                        if(c == RVX_EDGE_L)
                        {
                            while(voxel_copy[GetOffset(sx, y + 1, ez - height, model, inversep, sizes)] == e.top_left_col)
                            {
                                height++;
                            }
                        }
                        else if(c == RVX_EDGE_R)
                        {
                            while(voxel_copy[GetOffset(x, y + 1, ez - height, model, inversep, sizes)] == e.top_right_col)
                            {
                                height++;
                            }
                        }

                        // we have sx/ex/sz/ez
                        // now find the other edge
                        int ey = y + 1;
                        while(voxel_copy[GetOffset(sx, ey, sz, model, inversep, sizes)] != c)
                        {
                            ey++;
                        }

                        // now we have ey
                        e.sx          = sx + dx;
                        e.ex          = x + dx;
                        e.sy          = y + dy + 1;
                        e.ey          = ey + dy;
                        e.sz          = sz + dz;
                        e.ez          = ez + dz;
                        e.m_no        = en++;
                        e.area_no     = area.m_no;
                        e.spacing     = spacing;
                        e.edge_width  = width;
                        e.edge_height = height;
                        e.edge_dir    = (c == RVX_EDGE_L ? -1 : 1);
                        scene.m_edges.push_back(e);

                        int ry = y;
                        while(ry <= ey)
                        {
                            // remove all voxels in the edge area
                            int rz = sz;
                            while(rz <= ez)
                            {
                                RemoveVoxels(voxel_copy, sx, x, ry, rz, model, inversep, sizes);
                                rz++;
                            }
                            ry++;
                        }
                        continue;
                    }
#endif

                    if(optimize)
                    {
                        // see how far we can draw on X axis (line)
                        while(x < space_size_x - 1 && voxel_copy[GetOffset(x + 1, y, z, model, inversep, sizes)] == c)
                        {
                            x++;
                        }
                        // see if we can also extend Z axis (rectangle)
                        while(ez < space_size_z - 1 && AllVoxelsSameColor(voxel_copy, sx, x, y, ez + 1, model, inversep, c, sizes))
                        {
                            // fill in with zeros so we don't redraw later
                            RemoveVoxels(voxel_copy, sx, x, y, ez + 1, model, inversep, sizes);
                            ez++;
                        }
                    }

                    const int    _sx = sx + dx;
                    const int    _ex = x + dx;
                    const int    _y  = y + dy;
                    const int    _sz = sz + dz;
                    const int    _ez = ez + dz;
                    const int8_t ext = 0;
                    area.m_voxels.emplace_back(Voxel(c, (int16_t)_sx, (int16_t)_ex, (int16_t)_y, (int16_t)_sz, (int16_t)_ez, ext));
                }
            }
        }
        free(voxel_copy);
    }
}

struct scene_dim
{
    int   size_x;
    int   size_y;
    int   size_z;
    float tran_x;
    float tran_y;
    float tran_z;
};

const ogt_vox_scene* VOXLoader::GenerateBox(int x, int y, int z, bool roof, int margin)
{
    auto  vox       = const_cast<ogt_vox_scene*>(LoadVOX("resources/box.vox", false));
    auto& instance  = vox->instances[0];
    auto& model     = vox->models[instance.model_index];
    auto& transform = vox->instances[0].transform;

    const int cx = 0;
    const int cy = 0;

    x += margin * 2;
    y += margin * 2;
    z += margin * 2;

    std::vector<scene_dim> scene_dims;

    for(int ix = 0; ix < x; ix += 256)
        for(int iy = 0; iy < y; iy += 256)
            for(int iz = 0; iz < z; iz += 256)
            {
                scene_dim sd;
                sd.size_x = (ix + 256) < x ? 256 : x - ix;
                sd.size_y = (iy + 256) < y ? 256 : y - iy;
                sd.size_z = (iz + 256) < z ? 256 : z - iz;

                sd.tran_x = ix + sd.size_x / 2 + sd.size_x % 2 - margin;
                sd.tran_y = iy + sd.size_y / 2 + sd.size_y % 2 - margin;
                sd.tran_z = iz + sd.size_z / 2 + sd.size_z % 2 - margin;

                scene_dims.push_back(sd);
            }

    int scene_count = scene_dims.size();

    // count the number of required models, instances in the master scene
    uint32_t max_layers    = 1; // we don't actually merge layers. Every instance will be in layer 0.
    uint32_t max_models    = 0;
    uint32_t max_instances = 0;
    uint32_t max_groups    = 1; // we add 1 root global group that everything will ultimately be parented to.
    for(uint32_t scene_index = 0; scene_index < scene_count; scene_index++)
    {
        //if(!scenes[scene_index])
        //  continue;
        max_instances += vox->num_instances;
        max_models += vox->num_models;
        max_groups += vox->num_groups;
    }

    // allocate the master instances array
    ogt_vox_instance* instances     = (ogt_vox_instance*)_vox_malloc(sizeof(ogt_vox_instance) * max_instances);
    ogt_vox_model**   models        = (ogt_vox_model**)_vox_malloc(sizeof(ogt_vox_model*) * max_models);
    ogt_vox_layer*    layers        = (ogt_vox_layer*)_vox_malloc(sizeof(ogt_vox_layer) * max_layers);
    ogt_vox_group*    groups        = (ogt_vox_group*)_vox_malloc(sizeof(ogt_vox_group) * max_groups);
    uint32_t          num_instances = 0;
    uint32_t          num_models    = 0;
    uint32_t          num_layers    = 0;
    uint32_t          num_groups    = 0;

    // add a single layer.
    layers[num_layers].hidden = false;
    layers[num_layers].name   = "merged";
    num_layers++;

    // magicavoxel expects exactly 1 root group, so if we have multiple scenes with multiple roots,
    // we must ensure all merged scenes are parented to the same root group. Allocate it now for the
    // merged scene.
    uint32_t global_root_group_index = num_groups;
    {
        assert(global_root_group_index == 0);
        ogt_vox_group root_group;
        root_group.hidden             = false;
        root_group.layer_index        = 0;
        root_group.parent_group_index = k_invalid_group_index;
        root_group.transform          = _vox_transform_identity();
        groups[num_groups++]          = root_group;
    }

    // go ahead and do the merge now!
    size_t string_data_size = 0;
    for(uint32_t scene_index = 0; scene_index < scene_count; scene_index++)
    {
        const auto&          dim   = scene_dims[scene_index];
        const ogt_vox_scene* scene = vox;
        if(!scene)
            continue;

        // cache away the base model index for this scene.
        uint32_t base_model_index = num_models;
        uint32_t base_group_index = num_groups;

        // create copies of all models that have color indices remapped.
        for(uint32_t model_index = 0; model_index < scene->num_models; model_index++)
        {
            uint32_t voxel_count = dim.size_x * dim.size_y * dim.size_z;
            // clone the model
            ogt_vox_model* override_model      = (ogt_vox_model*)_vox_malloc(sizeof(ogt_vox_model) + voxel_count);
            uint8_t*       override_voxel_data = (uint8_t*)&override_model[1];

            // remap all color indices in the cloned model so they reference the master palette now!
            uint32_t voxel_index = 0;
            if(scene_index == 1)
            {
                int zz = 0;
                zz++;
            }
            for(int vz = 0; vz < dim.size_z; vz++)
            {
                auto abs_z  = vz + ((int)dim.tran_z - dim.size_z / 2);
                int  z_edge = (vz == 0 || vz == dim.size_z - 1) ? 1 : 0;
                int  z_div  = abs_z % 10 == 0 ? 1 : 0;
                for(int vy = 0; vy < dim.size_y; vy++)
                {
                    auto abs_y  = vy + ((int)dim.tran_y - dim.size_y / 2);
                    int  y_edge = (vy == 0 || vy == dim.size_y - 1) ? 1 : 0;
                    int  y_div  = (abs_y % 10 == 0 || vy == dim.size_y - 1) ? 1 : 0;
                    for(int vx = 0; vx < dim.size_x; vx++)
                    {
                        auto abs_x  = vx + ((int)dim.tran_x - (dim.size_x / 2));
                        int  x_edge = 0;
                        int  x_div  = abs_x % 10 == 0 ? 1 : 0;

                        bool floor      = (vz < margin);
                        bool ceil       = roof && (vz > z - margin - 1);
                        bool back_wall  = margin == 0 || (vy >= dim.size_y - margin);
                        bool side_wall  = abs_x < 2 || abs_x >= (x - margin * 2) - 2;
                        int  checkColor = (x_div + y_div + z_div > 1) ? 14 : 15 /* + rand() % 2*/;
                        //int checkColor = 

                        uint8_t finalColor = 0;
                        if(vy % 2 != 1)
                        {
                            if(vy < margin / 2)
                            {
                                // do not render front margin (unless we were on floor)
                                //override_voxel_data[voxel_index++] = 0;
                            }
                            else
                            {
                                uint8_t col = 0;
                                if(x_edge + y_edge + z_edge > 1)
                                    col = 16;
                                else if(floor)
                                {
                                    if((abs_x < margin * 2 || abs_x >= x - 4 * margin))
                                    {
                                        col = 8;
                                    }
                                    else
                                    {
                                        col = vy % 4 == 2 ? 15 : 13;
                                    }
                                    if(vy == 4)
                                        col = 8;
                                    if(vy == 2)
                                        col = 14;
                                }
                                else if(back_wall || side_wall)
                                {
                                    col = 15;
                                    if(vz == 4 || vz == 5)
                                        col = 14;
                                    if(vz > 5 && vz < 20)
                                        col = 7;

                                    if(vz >= dim.size_z - margin - 2)
                                        col = 14;
                                    else if(vz >= dim.size_z - margin - 6)
                                        col = 13;
                                    else if(vz >= dim.size_z - margin - 8)
                                        col = 14;
                                    else if(vz >= dim.size_z - margin - 10)
                                        col = 7;
                                    else if(vz >= dim.size_z - margin - 12)
                                        col = 14;
                                }

                                if(!floor && back_wall && (abs_x < margin || abs_x >= x - 3 * margin))
                                {
                                    col = 14;
                                    if(vz < margin)
                                    {
                                        col = 13;
                                    }
                                }

                                if(vy == 2)
                                {
                                    if(abs_x < -margin / 2 || abs_x > x - 2 * margin + margin / 2)
                                    {
                                        col = 13;
                                    }
                                    else if(abs_x < 0 || abs_x > x - 2 * margin)
                                    {
                                        col = 14;
                                    }
                                    if(vz < margin / 2)
                                    {
                                        col = 13;
                                    }
                                }

                                // above roof
                                if(vz >= dim.size_z - margin)
                                    col = 0;

                                finalColor = col;
                            }
                        }

                        override_voxel_data[voxel_index++] = finalColor;
                    }
                }
            }

            override_model->size_x     = dim.size_x;
            override_model->size_y     = dim.size_y;
            override_model->size_z     = dim.size_z;
            override_model->voxel_data = override_voxel_data;
            override_model->voxel_hash = _vox_hash(override_voxel_data, voxel_count);

            models[num_models++] = override_model;
        }

        // compute the scene bounding box on x dimension. this is used to offset instances
        // and groups in the merged model along X dimension such that they do not overlap
        // with instances from another scene in the merged model.
        int32_t scene_min_x = 0, scene_max_x = dim.size_x;
        //compute_scene_bounding_box_x(scene, scene_min_x, scene_max_x);
        //float scene_offset_x = (float)(offset_x - scene_min_x);
        float scene_offset_x = dim.tran_x; //        scene_index * 2; // TODO: fix;

        // each scene has a root group, and it must the 0th group in its local groups[] array,
        assert(scene->groups[0].parent_group_index == k_invalid_group_index);
        // create copies of all groups into the merged scene (except the root group from each scene -- which is why we start group_index at 1 here)
        for(uint32_t group_index = 1; group_index < scene->num_groups; group_index++)
        {
            const ogt_vox_group* src_group = &scene->groups[group_index];
            assert(src_group->parent_group_index !=
                   k_invalid_group_index); // there can be only 1 root group per scene and it must be the 0th group.
            ogt_vox_group dst_group = *src_group;
            assert(dst_group.parent_group_index < scene->num_groups);
            dst_group.layer_index = 0;
            dst_group.parent_group_index =
                (dst_group.parent_group_index == 0) ? global_root_group_index : base_group_index + (dst_group.parent_group_index - 1);
            // if this group belongs to the global root group, it must be translated so it doesn't overlap with other scenes.
            // if(dst_group.parent_group_index == global_root_group_index)
            //   dst_group.transform.m30 += scene_offset_x;
            groups[num_groups++] = dst_group;
        }

        // create copies of all instances (and bias them such that minimum on x starts at zero)
        for(uint32_t instance_index = 0; instance_index < scene->num_instances; instance_index++)
        {
            const ogt_vox_instance* src_instance = &scene->instances[instance_index];
            assert(src_instance->group_index < scene->num_groups); // every instance must be mapped to a group.
            ogt_vox_instance* dst_instance = &instances[num_instances++];
            *dst_instance                  = *src_instance;
            dst_instance->layer_index      = 0;
            dst_instance->group_index =
                (dst_instance->group_index == 0) ? global_root_group_index : base_group_index + (dst_instance->group_index - 1);
            dst_instance->model_index += base_model_index;
            if(dst_instance->name)
                string_data_size += _vox_strlen(dst_instance->name) + 1; // + 1 for zero terminator
            // if this instance belongs to the global rot group, it must be translated so it doesn't overlap with other scenes.
            if(dst_instance->group_index == global_root_group_index)
            {
                dst_instance->transform.m30 = dim.tran_x;
                dst_instance->transform.m31 = dim.tran_y;
                dst_instance->transform.m32 = dim.tran_z;
                if(margin == 0)
                {
                    dst_instance->transform.m31 = 0;
                    dst_instance->transform.m32 -= z;
                    //dst_instance->transform.m32 *= -1;
                }
            }
        }
    }

    // assign the master scene on output. string_data is part of the scene allocation.
    size_t         scene_size   = sizeof(ogt_vox_scene) + string_data_size;
    ogt_vox_scene* merged_scene = (ogt_vox_scene*)_vox_calloc(scene_size);

    // copy name data into the string section and make instances point to it. This makes the merged model self-contained.
    char* scene_string_data = (char*)&merged_scene[1];
    for(uint32_t instance_index = 0; instance_index < num_instances; instance_index++)
    {
        if(instances[instance_index].name)
        {
            size_t string_len = _vox_strlen(instances[instance_index].name) + 1; // +1 for zero terminator
            memcpy(scene_string_data, instances[instance_index].name, string_len);
            instances[instance_index].name = scene_string_data;
            scene_string_data += string_len;
        }
    }

    assert(num_groups <= max_groups);

    memset(merged_scene, 0, sizeof(ogt_vox_scene));
    merged_scene->instances     = instances;
    merged_scene->num_instances = max_instances;
    merged_scene->models        = (const ogt_vox_model**)models;
    merged_scene->num_models    = max_models;
    merged_scene->layers        = layers;
    merged_scene->num_layers    = max_layers;
    merged_scene->groups        = groups;
    merged_scene->num_groups    = num_groups;

    for(uint32_t color_index = 0; color_index < 256; color_index++)
        merged_scene->palette.color[color_index] = vox->palette.color[color_index];

    return merged_scene;
}

void VOXLoader::ExportVOX(const ogt_vox_scene* vox, const char* fileName)
{
    save_vox_scene(fileName, vox);
}

std::vector<Vector2> VOXLoader::GenerateRoomSlices(int fw, int fh, int nw, int nh, int d)
{
    std::vector<Vector2> sv;

    float _w     = nw;
    float _h     = nh;
    float _stepw = (fw - nw) / (float)d;
    float _steph = (fh - nh) / (float)d;
    for(int _d = 0; _d < d; _d++)
    {
        sv.push_back(Vector2 {_w, _h});
        _w += _stepw;
        _h += _steph;
    }

    return sv;
}

} // namespace rvx