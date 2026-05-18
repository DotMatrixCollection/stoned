require "bundler"

module Bundler
  module SharedHelpers
    class << self
      def root
        Bundler.root
      rescue Bundler::GemfileNotFound
        Pathname.new(Dir.pwd)
      end

      def pwd
        root
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

      def default_bundle_dir
        Bundler.bundle_path
      end

      def files_in_use
        {
          gemfile: Bundler.default_gemfile,
          lockfile: Bundler.default_lockfile,
        }
      end

      def chdir(path, &block)
        Dir.chdir(path, &block)
      end
    end
  end
end
