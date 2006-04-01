#!/usr/bin/env ruby

require 'wmlib'
require 'gnome2'

# WM::Window.each {|x| puts "#{x.title}" }

#WM::Window.on_press("a")
#WM::Window.key_loop

include WM

=begin
if Desktop.current == Desktop.last
  Desktop.current = 0
else
  Desktop.current = Desktop.current + 1
end

if Desktop.current == 0
  Desktop.current = Desktop.last
else
  Desktop.current = Desktop.current - 1
end
=end
