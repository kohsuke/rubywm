#!/usr/bin/env ruby

require File.dirname($0)+'/wmlib.so'
#require $0+'/../wmlib.so'
require 'set'
require 'ostruct'

class Point
    attr_accessor :x,:y
    def initialize(x,y)
        @x = x
        @y = y
    end
    def <= (rhs)
        @x <= rhs.x && @y <=rhs.y
    end
    def / (rhs)
        Point.new(@x/rhs,@y/rhs)
    end
    def * (rhs)
        Point.new(@x*rhs,@y*rhs)
    end
    def + (rhs)
        Point.new(@x+rhs.x,@y+rhs.y)
    end
    def - (rhs)
        Point.new(@x-rhs.x,@y-rhs.y)
    end
    def direction()
        if @x>@y then
            if -@x>@y then
                :up
            else
                :right
            end
        else
            if -@x>@y then
                :left
            else
                :down
            end
        end
    end
    def length()
        Math.sqrt((@x*@x)+(@y*@y))
    end
    def area()
        return @x*@y
    end
    def to_s()
        "(#{@x},#{@y})"
    end
end

class Wnd
    attr_accessor :x1,:y1,:x2,:y2,:w
    def initialize(w)
        @x1 = w.left
        @y1 = w.top
        @x2 = w.right
        @y2 = w.bottom
        @w = w
        @title = w.title
        @tl = Point.new(@x1,@y1)
        @br = Point.new(@x2,@y2)
    end

    # is the given region within this window?    
    def contains(x1,y1,x2,y2)
        @x1<=x1 && @y1<=y1 && x2<=@x2 && y2<=@y2
    end
    
    # size of the window
    def size
        (@x2-@x1)*(@y2-@y1)
    end
    
    def activate()
        @w.activate
    end
    
    def to_s()
        @title
    end
end

class Screen
    attr_accessor :xs,:ys,:windows,:screen
    def initialize(windows)
        @windows = windows
        # compute interesting boundaries
        @xs = windows.collect { |w| [w.x1,w.x2] }.flatten.sort.uniq
        @ys = windows.collect { |w| [w.y1,w.y2] }.flatten.sort.uniq
        # create 2D array of xs.size x ys.size 
        @screen = Array.new(@xs.size-1).collect { Array.new(@ys.size-1) }
        windows.each { |w|
            eachRegion { |x,y,wnd|
                if w.contains(@xs[x],@ys[y],@xs[x+1],@ys[y+1]) then
                    @screen[x][y] = w
                end
            }
        }
    end
    
    def eachRegion(&body)
        (0...(@ys.size-1)).each { |y|
            (0...(@xs.size-1)).each { |x|
                yield x,y,@screen[x][y]
            }
        }
    end
    
    def area(x,y)
        (pt(x+1,y+1)-pt(x,y)).area
    end
    
    def pt(x,y)
        Point.new(@xs[x],@ys[y])
    end
    
    def center(x,y)
        pt(x,y)+pt(x+1,y+1)/2
    end
end


# list up all windows that we care
# this enumerates windows from bottom to top
windows = []
WM::Window.each {|x|
    if !x.visible? then
        next
    end
    if !(["gnome-panel","desktop_window","compiz"].include? x.winclass) then
        w = Wnd.new(x)
        windows << w
        puts "#{x.title} : #{x.winclass} (#{w.x1},#{w.y1},#{w.x2},#{w.y2})"
    end
}

s=Screen.new(windows)

# screen real estate map
(0...(s.ys.size-1)).each {|y|
    (0...(s.xs.size-1)).each {|x|
        w = s.screen[x][y]
        print w==nil ? '-' : w.to_s[0,1]
    }
    puts ""
}

# compute the center of gravity for visible region of each window
cog = windows.collect { |w|
    pixels = 0
    wp = Point.new(0,0)
    s.eachRegion { |x,y,wnd|
        if wnd==w then
            a = s.area(x,y)
            pixels += a
            wp += s.center(x,y)*a
        end
    }
    # puts "#{w} - #{wp/pixels} #{pixels}"
    
    OpenStruct.new({ :window => w, :center => pixels>0 ? wp/pixels : wp, :size =>pixels })
}

cog = cog.reverse  # top most first

# this is the current top-most window
cur = cog[0]

# ignore windows that are mostly invisible
cog = cog.delete_if { |x| x.size<40000 }


if ARGV.size==0 then
    # diagnostic output
    puts "Current windows is #{cur.window.to_s} (#{cur.window.w.winclass}) cog=#{cur.center}"
    cog[1..-1].each { |w|
        relp = w.center-cur.center
        puts "  #{w.window} #{relp} #{relp.direction} d:#{relp.length} s:#{w.size}"
    }
else
    target_dir = ARGV[0].to_sym
    t=cog[1..-1].select{ |w|  # of the windows that are in the right direction
        w!=cur && (w.center-cur.center).direction==target_dir
    }.min{|a,b| # pick up the nearest one
        (a.center-cur.center).length <=> (b.center-cur.center).length
    }
    # if a suitable one is found, set focus
    if t!=nil then
        t.window.activate
    end
end

