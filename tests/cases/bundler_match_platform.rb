$stdout.sync = true

require "bundler/match_platform"

puts Bundler::MatchPlatform.platform?("ruby")
puts Bundler::MatchPlatform.platform?(RUBY_PLATFORM)
puts Bundler::MatchPlatform.platform?("definitely-not-a-platform") == false
