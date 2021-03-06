#include <string>
#include <opencv2/contrib/contrib.hpp> 
#include <iostream>
#include "vector"
#include "../h/ClueBox.h"

using namespace cv;
using namespace std;

ClueBox::ClueBox(int number, int width, int height, int item_width, int item_height)	//_clue_number,_box_width,_box_height
{
	_clue_number = number;
	_box_width = width;
	_box_height = height;
	_item_width = item_width;
	_item_height = item_height;
	_item_selected = -1;
}


void ClueBox::set_box_width(int width){
	_box_width = width;
}

void ClueBox::set_box_height(int height)
{
	_box_height = height;
}

void ClueBox::set_item_width(int width)
{
	_item_width = width;
}

void ClueBox::set_item_height(int height)
{
	_item_height = height;
}

void ClueBox::set_item_selected(int index)
{
	if (index<=_clue_number)
		_item_selected = index;
}


int ClueBox::get_clue_number()
{
	_clue_number = _clue_vector.size();
	return _clue_number;
}

int ClueBox::get_box_width()
{
	return _box_width;
}

int ClueBox::get_box_height()
{
	return _box_height;
}

int ClueBox::get_item_width()
{
	return _item_width;
}

int ClueBox::get_item_height()
{
	return _item_height;
}

int ClueBox::get_item_selected()
{
	return _item_selected;
}

string ClueBox::get_item_name(int index){
	return _clue_vector[index].clue_name();
}


void ClueBox::show_clue_box(Mat image)	//用clue array裡面存的clue選圖出來show
{
	//printf("show_clue_box\n");
	renderBackgroundGL(image,0,0,1,0.15);
}


void ClueBox::InsertItem(Clue clue)
{
	_clue_vector.push_back(clue);
	_clue_number = sizeof(_clue_vector) / sizeof(_clue_vector[0]);
}


void ClueBox::DelItem(int index)
{
	vector< Clue >::iterator itor;
	itor = _clue_vector.begin() + index - 1;
	_clue_vector.erase(itor);
	_clue_number = sizeof(_clue_vector) / sizeof(_clue_vector[0]);
}


void selectItem(int index,int width,int height){
	float horizon_space = SPACE*height / width;
	float item_w = ITEM_WIDTH*height / width;
	float vertical_space = SPACE;
	float item_h = ITEM_WIDTH;
	float x1, x2, y1, y2;
	Mat image = imread("D:\\resource\\paper_texture_small.png");
	x1 = ARROW_WIDTH + index*horizon_space + (index - 1)*item_w;
	y1 = vertical_space;
	x2 = ARROW_WIDTH + index*horizon_space + index*item_w;
	y2 = vertical_space + item_h;
	
	// Make sure that the polygon mode is set so we draw the polygons filled
	// (save the state first so we can restore it).

	GLint polygonMode[2];
	glGetIntegerv(GL_POLYGON_MODE, polygonMode);
	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_FILL);

	// Set up the virtual camera, projecting using simple ortho so we can draw the background image.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, 1.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Draw the image.
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_BLEND);								//Enable blending.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //Set blending function.

	glBegin(GL_QUADS);
	glColor4f(0, 0, 0, 0.3);

		glVertex3f(x1, y1, 0.0);
		glVertex3f(x1 , y2, 0.0);
		glVertex3f(x2 , y2, 0.0);
		glVertex3f(x2, y1, 0.0);
	
	glEnd();

	// Clear the depth buffer so the texture forms the background.
	glClear(GL_DEPTH_BUFFER_BIT);

	// Restore the polygon mode state.
	glPolygonMode(GL_FRONT, polygonMode[0]);
	glPolygonMode(GL_BACK, polygonMode[1]);
}


void ClueBox::show_clue(int width, int height)
{
	vector<Clue>::iterator it_i;
	float horizon_space = SPACE*height / width;
	float item_w = ITEM_WIDTH*height / width;
	float vertical_space = SPACE;
	float item_h = ITEM_WIDTH;
	Mat image = imread("D:\\resource\\paper_texture_small.png");
	int i = 1;
	for (it_i = _clue_vector.begin(); it_i != _clue_vector.end() && (ARROW_WIDTH + i*horizon_space + i*item_w)<0.98; ++it_i){
		renderBackgroundGL(it_i->get_cluebox_img(), ARROW_WIDTH + i*horizon_space + (i - 1)*item_w, vertical_space, ARROW_WIDTH + i*horizon_space + i*item_w, vertical_space + item_h);

		//change the cluebox_on_the_screen vector here
		i++;
	}
	i = 1;
	//renderBackgroundGL(image, ARROW_WIDTH + i*horizon_space + (i - 1)*item_w, vertical_space, ARROW_WIDTH + i*horizon_space + i*item_w, vertical_space + item_h);

	if (_item_selected != -1){
		selectItem(_item_selected+1,width,height);
	}
	set_item_show_last(i - 1);
}


void ClueBox::set_item_show_first(int toward)
{
	if (toward == 1 && _item_show_first<_clue_number)
		_item_show_first +=1;
	else if (toward == 0 && _item_show_first>1)
		_item_show_first -= 1;
}


void ClueBox::set_item_show_last(int index)
{
	_item_show_last = index;
}


ostream& operator<<(ostream& os, const ClueBox& cluebox)
{
	int _clue_number;
	int _box_width;
	int _box_height;
	int _item_width;
	int _item_height;
	int _item_selected;
	//os << dt.mo << '/' << dt.da << '/' << dt.yr;
	os << "_clue_number = " << cluebox._clue_number << "\n"\
		"_box_width = " << cluebox._box_width << "\n"\
		"_box_height = " << cluebox._box_height << "\n"\
		"_item_width = " << cluebox._item_width << "\n"\
		"_item_height = " << cluebox._item_height << "\n"\
		"_item_selected = " << cluebox._item_selected << "\n" << endl;

	return os;
}
