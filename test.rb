#!/usr/bin/env ruby

require 'wmlib'

include WM

for window in Window
  puts "#{window} - #{window.winclass}"
end
