require "bundler"

module Bundler
  class CurrentRuby
    def ruby_version
      RUBY_VERSION
    end

    def ruby_platform
      RUBY_PLATFORM
    end

    def to_s
      "#{ruby_version} (#{ruby_platform})"
    end
  end

  def self.current_ruby
    @current_ruby ||= CurrentRuby.new
  end
end
