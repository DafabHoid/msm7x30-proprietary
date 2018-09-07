
void jpeg_dump_htable(void* table)
{
	// Compiled out
}
void jpeg_dump_qtable(void* table)
{
	// Compiled out
}
void jpeg_dump_entropy_selector(void*)
{
	// Compiled out
}
void jpeg_dump_comp_info(void*)
{
	// Compiled out
}
void jpeg_show_leak()
{
	// Compiled out
}

void jpeg_dump_header(struct jpeg_header* header)
{
	if (header) {
		jpeg_dump_frame_info(header->frame1);
		jpeg_dump_frame_info(header->frame2);
	}
}

void jpeg_scan_info_destroy(struct scan_info* sinfo)
{
	if (sinfo) {
		jpeg_free(sinfo->mem);
		sinfo->mem = NULL;
		jpeg_free(sinfo);
	}
}

void jpeg_header_destroy(struct jpeg_header* header)
{
	if (header) {
		jpeg_frame_info_destroy(header->frame1);
		jpeg_frame_info_destroy(header->frame2);
		exif_destroy(&header->exif);
		header->frame1 = NULL;
		header->frame2 = NULL;
	}
}

void jpeg_frame_info_destroy(struct frame_info* finfo)
{
	if (finfo) {
		for (int i = 0; i < 4; ++i) {
			if (finfo->qtable[i])
				jpeg_free(finfo->qtable[i]);
			finfo->qtable[i] = NULL;
		}
		if (finfo->scan_infos) {
			for (unsigned char i = 0; i < finfo->scan_info_count; ++i) {
				jpeg_scan_info_destroy(finfo->scan_infos[i]);
			}
			jpeg_free(finfo->scan_infos);
			finfo->scan_infos = NULL;
		}
		if (finfo->ptr1) {
			jpeg_free(finfo->ptr1);
			finfo_ptr1 = NULL;
		}
		jpeg_free(finfo);
	}
}
