require "bundler"

module Bundler
  module MatchPlatform
    class << self
      def platform?(platform)
        platform = platform.to_s
        return true if platform == "ruby"
        RUBY_PLATFORM.include?(platform)
      end
    end
  end
end
