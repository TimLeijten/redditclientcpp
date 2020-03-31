#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//#define Dprinthttpscodes
//#define Dprinthttpsresponsedata
#include "httpsc.h"

#define rj rapidjson

#define windowWidth  1080
#define windowHeight 1920

class RedditPost
{
public:
    RedditPost(rj::Value& item)
    {
        kind = item["kind"].GetString();

        rj::Value& data = item["data"];

        subredditNamePrefixed = data["subreddit_name_prefixed"].GetString();
        title = data["title"].GetString();
        domain = data["domain"].GetString();
        upvotes = data["ups"].GetInt();
        isTextPost = data["is_self"].GetBool();
        author = data["author"].GetString();
        numComments = data["num_comments"].GetInt();
        url = data["url"].GetString();
        selfText = data["selftext"].GetString();
        thumbnailUrl = data["thumbnail"].GetString();

        isImagePost = false;
        isLinkPost = false;
        isRedditVideo = false;

        if (domain == "i.redd.it")
        {
            isImagePost = true;
        }
        else if (domain == "i.imgur.com")
        {
            isImagePost = true;
        }
        else if (domain == "media.giphy.com")
        {
            isImagePost = true;
        }
        else if (domain == "v.redd.it")
        {
            isRedditVideo = true;
        }
        else if (!isTextPost)
        {
            isLinkPost = true;
        }
    }
    std::string kind;
    std::string domain;
    int upvotes;
    bool isTextPost;
    bool isImagePost;// Means it's an image that can be directly downloaded
    bool isRedditVideo;// Means it's a video hosted by reddit.
    bool isLinkPost;
    std::string url;/// Link to webpage, or image if isImage, or self is isText.
    int numComments;
    std::string subredditNamePrefixed;
    std::string title;
    std::string selfText;
    std::string author;
    std::string thumbnailUrl;
};

class ScrollBar
{
public:
    ScrollBar(int _paddleSize, int _scrollBarThickness)
    {
        paddleSize = _paddleSize;
        scrollBarThickness = _scrollBarThickness;
        position = 0;
    }
    void drawScrollBar(SDL_Renderer *renderer)
    {
        for (int i = 0; i < scrollBarThickness; i++)
        {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderDrawLine(renderer, windowWidth-(1*i), 0, windowWidth-(1*i), windowHeight);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, windowWidth-(1*i), (int)(position*((float)windowHeight)), windowWidth-(1*i), (position*windowHeight)+paddleSize);
        }

    }
    void setScrollbarPos(float pos)
    {
        position = pos;
    }
    void scrollBarInput(SDL_Event *event)
    {
        /*if (event->type == SDL_MOUSEMOTION)
        {
            if (event->motion.x)
        }*/
    }
    int paddleSize;
    int scrollBarThickness;
    float position;// 0-1
};

struct sdlText
{
    SDL_Rect rect;
    SDL_Texture *texture;
};

struct sdlImage
{
    SDL_Texture* tex;
    SDL_Rect rect;
    bool success;
    bool isGif;
    std::vector<SDL_Texture*> gifTextures;
    int frameCount;
    int currentFrame;
};

class sdlLine
{
public:
    sdlLine();
    sdlLine(int _x, int _y, int _x2, int _y2)
    {
        x = _x;
        y = _y;
        x2 = _x2;
        y2 = _y2;
    }
    int x;
    int y;
    int x2;
    int y2;
};

void get_text_and_rect(SDL_Renderer *renderer, int x, int y, std::string text,
        TTF_Font *font, SDL_Texture **texture, SDL_Rect *rect) {
    int text_width;
    int text_height;
    SDL_Surface *surface;
    SDL_Color textColor = {255, 255, 255, 0};

    surface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), textColor, windowWidth);
    *texture = SDL_CreateTextureFromSurface(renderer, surface);
    text_width = surface->w;
    text_height = surface->h;
    SDL_FreeSurface(surface);
    rect->x = x;
    rect->y = y;
    rect->w = text_width;
    rect->h = text_height;
}

sdlImage* loadNormImageFromMemory(SDL_Renderer* renderer, std::string Imdata)
{
    sdlImage *img = new sdlImage();
    img->success = true;

    int req_format = STBI_rgb_alpha;
    int width, height, orig_format;
    //unsigned char* Imagedata = (unsigned char*)malloc(Imdata.size());
    //strcpy((char*)Imdata.c_str(), Imdata.size());
    unsigned char* data = stbi_load_from_memory((unsigned char*)Imdata.c_str(), Imdata.size(), &width, &height, &orig_format, req_format);
    if (data == NULL) {
        std::cout << "Loading image failed: " <<  stbi_failure_reason() << std::endl;
        //exit(1);
        img->success = false;
    }
    int depth, pitch;
    Uint32 pixel_format;
    int bpp = 0;
    if (req_format == STBI_rgb) {
        depth = 24;
        pitch = 3*width; // 3 bytes per pixel * pixels per row
        pixel_format = SDL_PIXELFORMAT_RGB24;
        bpp = 3;
    } else { // STBI_rgb_alpha (RGBA)
        depth = 32;
        pitch = 4*width;
        pixel_format = SDL_PIXELFORMAT_RGBA32;
        bpp = 4;
    }

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height,
                                                           depth, pitch, pixel_format);
    if (surf == NULL) {
        std::cout << "Creating surface failed: " << SDL_GetError() << std::endl;
        //stbi_image_free(data);
        //exit(1);
        img->success = false;
    }

    img->tex = SDL_CreateTextureFromSurface(renderer, surf);
    img->rect.x = 0;
    img->rect.y = 0;
    img->rect.w = width;
    img->rect.h = height;

    SDL_FreeSurface(surf);
    stbi_image_free(data);
    img->isGif = false;
    return img;
}

sdlImage* loadGifImageFromMemory(SDL_Renderer* renderer, std::string Imdata)
{
    sdlImage *img = new sdlImage();
    img->success = true;

    int req_format = STBI_rgb_alpha;
    int width, height, orig_format, z;
    //unsigned char* Imagedata = (unsigned char*)malloc(Imdata.size());
    //strcpy((char*)Imdata.c_str(), Imdata.size());
    unsigned char* data = stbi_load_gif_from_memory((unsigned char*)Imdata.c_str(), Imdata.size(), 0, &width, &height, &z, &orig_format, req_format);
    if (data == NULL) {
        std::cout << "Loading image failed: " <<  stbi_failure_reason() << std::endl;
        //exit(1);
        img->success = false;
    }
    int depth, pitch;
    Uint32 pixel_format;
    int bpp = 0;
    if (req_format == STBI_rgb) {
        depth = 24;
        pitch = 3*width; // 3 bytes per pixel * pixels per row
        pixel_format = SDL_PIXELFORMAT_RGB24;
        bpp = 3;
    } else { // STBI_rgb_alpha (RGBA)
        depth = 32;
        pitch = 4*width;
        pixel_format = SDL_PIXELFORMAT_RGBA32;
        bpp = 4;
    }
    for (int i = 0; i < z; i++)
    {
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom((void*)(data + (width*height*bpp*i)), width, height,
                                                               depth, pitch, pixel_format);
        if (surf == NULL) {
            std::cout << "Creating surface failed: " << SDL_GetError() << std::endl;
            //stbi_image_free(data);
            //exit(1);
            img->success = false;
        }

        img->gifTextures.push_back(SDL_CreateTextureFromSurface(renderer, surf));
        SDL_FreeSurface(surf);
    }
    img->frameCount = z;
    img->currentFrame = 0;
    img->tex = 0;
    img->rect.x = 0;
    img->rect.y = 0;
    img->rect.w = width;
    img->rect.h = height;

    stbi_image_free(data);
    img->isGif = true;
    return img;
}

sdlImage* loadImageFromMemory(SDL_Renderer* renderer, std::string Imdata)
{
    if (Imdata[0] == 71 && Imdata[1] == 73 && Imdata[2] == 70)
    {
        return loadGifImageFromMemory(renderer, Imdata);
    }
    else
    {
        return loadNormImageFromMemory(renderer, Imdata);
    }
}

int main()
{
    std::string subreddit = "dankmemes";

    // Fetch json from reddit:
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    boost::asio::io_service io_service;
    client c(io_service, ctx, "www.reddit.com", "/r/" + subreddit + "/hot.json?limit=20");
    io_service.run();

    // Parse json:
    rj::Document d;
    d.Parse(c.responseData.c_str());
    if (d.HasParseError())
    {
        std::cerr << "Parse Error!:" << d.GetParseError() << std::endl;
    }
    std::string kind = d["kind"].GetString();
    std::cout << "Kind: " << kind << std::endl;

    int dist = d["data"]["dist"].GetInt();
    std::cout << "Dist: " << dist << std::endl;

    int childrenSize = d["data"]["children"].GetArray().Size();
    std::cout << "Children Size: " << childrenSize << std::endl;

    std::vector<RedditPost*> redditPosts;

    for (int i = 0; i < childrenSize; i++)
    {
        rj::Value& item = d["data"]["children"].GetArray()[i];
        /*std::cout << "Index: " << i << std::endl;
        std::cout << " Kind: " << item["kind"].GetString() << std::endl;
        std::cout << "  Data:" << std::endl;
        rj::Value& data = item["data"];
        std::cout << "   Subreddit Name Prefixed: " << data["subreddit_name_prefixed"].GetString() << std::endl;
        std::cout << "   Title: " << data["title"].GetString() << std::endl;
        std::cout << "   Domain: " << data["domain"].GetString() << std::endl;// Upvotes
        std::cout << "   Ups: " << data["ups"].GetInt() << std::endl;// Upvotes
        //std::cout << "   Self Text: " << data["selftext"].GetString() << std::endl;// Is actually at the top of data object but placed it here for convenience.
        std::cout << "   Is Self: " << data["is_self"].GetBool() << std::endl;// Means it's a text post
        std::cout << "   Author: " << data["author"].GetString() << std::endl;
        std::cout << "   Num Comments: " << data["num_comments"].GetInt() << std::endl;
        std::cout << "   Url: " << data["url"].GetString() << std::endl;
        std::cout << "   Is Video: " << data["is_video"].GetBool() << std::endl;// Means it's a reddit video only, this is false for e.g. a yt video.
        */

        RedditPost* post = new RedditPost(item);

        std::cout << "Index: " << i << std::endl;
        std::cout << " Kind: " << post->kind << std::endl;

        std::cout << "  Subreddit Name Prefixed: " << post->subredditNamePrefixed << std::endl;
        std::cout << "  Title: " << post->title << std::endl;
        std::cout << "  Domain: " << post->domain << std::endl;// Upvotes
        std::cout << "  Ups: " << post->upvotes << std::endl;// Upvotes
        //std::cout << "  Self Text: " << data["selftext"].GetString() << std::endl;// Is actually at the top of data object but placed it here for convenience.
        std::cout << "  Is Self: " << post->isTextPost << std::endl;// Means it's a text post
        std::cout << "  Author: " << post->author << std::endl;
        std::cout << "  Num Comments: " << post->numComments << std::endl;
        std::cout << "  Url: " << post->url << std::endl;
        std::cout << "  is Image Post: " << post->isImagePost << std::endl;
        std::cout << "  is Link Post: " << post->isLinkPost << std::endl;
        //std::cout << "  Is Video: " << data["is_video"].GetBool() << std::endl;// Means it's a reddit video only, this is false for e.g. a yt video.

        std::cout << std::endl;

        redditPosts.push_back(post);
    }

    // Create SDL window:
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "Failed to init sdl!" << std::endl;
    }
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    if (SDL_CreateWindowAndRenderer(windowWidth, windowHeight, 0, &window, &renderer) != 0)
    {
        std::cerr << "Failed to create window and renderer!" << std::endl;
    }

    TTF_Init();
    TTF_Font *font = TTF_OpenFont("arial.ttf", 28);
    if (font == NULL) {
        std::cerr << "Font not found!" << std::endl;
    }

    TTF_Font *fontPost = TTF_OpenFont("arial.ttf", 22);
    if (font == NULL) {
        std::cerr << "Font not found!" << std::endl;
    }

    TTF_Font *fontSmall = TTF_OpenFont("arial.ttf", 12);
    if (font == NULL) {
        std::cerr << "Small Font not found!" << std::endl;
    }

    std::vector<sdlText*> sdlTexts;
    std::vector<sdlImage*> sdlImages;
    std::vector<sdlLine*> sdlLines;
    int textOffset = 0;
    for (int i = 0; i < redditPosts.size(); i++)
    {
        sdlText *title = new sdlText();// Title text
        get_text_and_rect(renderer, 10, textOffset+30, redditPosts[i]->title.c_str(), font, &title->texture, &title->rect);
        sdlTexts.push_back(title);

        sdlText *sub = new sdlText();// Subreddit text
        get_text_and_rect(renderer, 10, textOffset, redditPosts[i]->subredditNamePrefixed.c_str(), font, &sub->texture, &sub->rect);
        sdlTexts.push_back(sub);

        sdlText *user = new sdlText();// Post user text
        get_text_and_rect(renderer, 10+sub->rect.w+20, textOffset+10, redditPosts[i]->author.c_str(), fontSmall, &user->texture, &user->rect);
        sdlTexts.push_back(user);

        // Apply offset based on height of title and sub text
        textOffset += title->rect.h;
        textOffset += sub->rect.h+5;

        if (redditPosts[i]->isTextPost && redditPosts[i]->selfText != "")// Display post text if applicable
        {
            sdlText *text = new sdlText();
            get_text_and_rect(renderer, 10, textOffset+30, redditPosts[i]->selfText.c_str(), fontPost, &text->texture, &text->rect);
            sdlTexts.push_back(text);
            textOffset += text->rect.h;
        }
        else if (redditPosts[i]->isImagePost && true)// Display post image if applicable
        {
            std::string imgUrl = redditPosts[i]->url;
            size_t pos = imgUrl.find("https://"+redditPosts[i]->domain);
            if (pos != std::string::npos)
            {
                imgUrl.replace(pos, std::string("https://"+redditPosts[i]->domain).length(), "");
                std::cout << redditPosts[i]->domain << " " << imgUrl << std::endl;

                boost::asio::io_service io_service2;
                client c(io_service2, ctx, redditPosts[i]->domain, imgUrl);
                io_service2.run();

                sdlImage* img = loadImageFromMemory(renderer, c.responseData);
                if (img->success)
                {
                    img->rect.y = textOffset+10;
                    img->rect.x = 10;

                    if (img->rect.w > windowWidth-10)
                    {
                        float aspect = img->rect.h / img->rect.w;// Calculate aspect ratio to scale iamge venly
                        img->rect.w = windowWidth-10-10;
                        img->rect.h = aspect*(windowWidth-10-10);
                    }

                    sdlImages.push_back(img);

                    textOffset += img->rect.h+10;
                }
            }
        }
        else if (redditPosts[i]->thumbnailUrl != "default" && redditPosts[i]->thumbnailUrl != "nsfw")// Display thumbnail when the post isn't an image post.
        {
            // thumbnail
            std::string imgUrl = redditPosts[i]->thumbnailUrl;
            size_t pos = imgUrl.find("https://b.thumbs.redditmedia.com");
            if (pos != std::string::npos)
            {
                imgUrl.replace(pos, std::string("https://b.thumbs.redditmedia.com").length(), "");
                std::cout << "b.thumbs.redditmedia.com" << " " << imgUrl << std::endl;

                boost::asio::io_service io_service2;
                client c(io_service2, ctx, "b.thumbs.redditmedia.com", imgUrl);
                io_service2.run();

                sdlImage* img = loadImageFromMemory(renderer, c.responseData);
                img->rect.y = textOffset - title->rect.h;
                img->rect.x = title->rect.w+20;
                if (img->rect.x+img->rect.w > windowWidth)
                {
                    img->rect.x = 10;
                    img->rect.y = textOffset + title->rect.h;
                    textOffset += img->rect.h + title->rect.h;
                }
                else
                {
                    textOffset += img->rect.h - title->rect.h;
                }

                sdlImages.push_back(img);

                //textOffset += img->rect.h - title->rect.h;
            }

            pos = imgUrl.find("https://a.thumbs.redditmedia.com");
            if (pos != std::string::npos)
            {
                imgUrl.replace(pos, std::string("https://a.thumbs.redditmedia.com").length(), "");
                std::cout << "a.thumbs.redditmedia.com" << " " << imgUrl << std::endl;

                boost::asio::io_service io_service2;
                client c(io_service2, ctx, "a.thumbs.redditmedia.com", imgUrl);
                io_service2.run();

                sdlImage* img = loadImageFromMemory(renderer, c.responseData);
                img->rect.y = textOffset - title->rect.h;
                img->rect.x = title->rect.w+20;
                if (img->rect.x+img->rect.w > windowWidth)
                {
                    img->rect.x = 10;
                    img->rect.y = textOffset + title->rect.h;
                    textOffset += img->rect.h + title->rect.h;
                }
                else
                {
                    textOffset += img->rect.h - title->rect.h;
                }

                sdlImages.push_back(img);

                //textOffset += img->rect.h - title->rect.h;
            }
        }
        sdlLine *line = new sdlLine(20, textOffset + 25, windowWidth-20, textOffset + 25);
        sdlLines.push_back(line);
        sdlLine *line2 = new sdlLine(20, textOffset + 26, windowWidth-20, textOffset + 26);
        sdlLines.push_back(line2);

        textOffset += 50;

    }

    ScrollBar scrollbar(40, 7);

    int yScroll = 0;

    bool windowOpen = true;
    int a = 0;
    while (windowOpen)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                windowOpen = false;
            }
            if (event.type == SDL_MOUSEWHEEL)
            {
                yScroll -= event.wheel.y*30;
            }
            //if (event.type == )
        }
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderClear(renderer);
        for (int i = 0; i < sdlTexts.size(); i++)
        {
            SDL_Rect rect = sdlTexts[i]->rect;
            rect.y -= yScroll;
            SDL_RenderCopy(renderer, sdlTexts[i]->texture, NULL, &rect);
        }
        a++;

        for (int i = 0; i < sdlImages.size(); i++)
        {
            if (sdlImages[i]->success)
            {
                if (sdlImages[i]->isGif == false)
                {
                    SDL_Rect rect = sdlImages[i]->rect;
                    rect.y -= yScroll;
                    SDL_RenderCopy(renderer, sdlImages[i]->tex, NULL, &rect);
                }
                else
                {
                    SDL_Rect rect = sdlImages[i]->rect;
                    rect.y -= yScroll;
                    //std::cout << sdlImages[i]->gifTextures.size() << "|" << sdlImages[i]->frameCount << std::endl;
                    SDL_RenderCopy(renderer, sdlImages[i]->gifTextures[sdlImages[i]->currentFrame], NULL, &rect);
                    if (a % 800 == 0)
                    {
                        sdlImages[i]->currentFrame++;
                    }

                    if (sdlImages[i]->currentFrame > sdlImages[i]->frameCount-1)
                    {
                        sdlImages[i]->currentFrame = 0;
                    }
                }
            }

        }

        for (int i = 0; i < sdlLines.size(); i++)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, sdlLines[i]->x, sdlLines[i]->y-yScroll, sdlLines[i]->x2, sdlLines[i]->y2-yScroll);
        }

        scrollbar.setScrollbarPos(((float)yScroll)/((float)textOffset));
        scrollbar.drawScrollBar(renderer);

        SDL_RenderPresent(renderer);

    }
}
