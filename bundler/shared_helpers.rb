require "bundler"

module Bundler
  module SharedHelpers
    class << self
      def pwd
        Bundler.root
      rescue Bundler::GemfileNotFound
        Pathname.new(Dir.pwd)
      end

      def default_gemfile
        Bundler.default_gemfile
      end

      def default_lockfile
        Bundler.default_lockfile
      end

      def in_bundle?
        File.exist?(Bundler.gemfile)
      rescue Bundler::GemfileNotFound
        false
      end
    end
  end
end
